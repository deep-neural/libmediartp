#include "h265_depacketizer.h"
#include <algorithm>

namespace rtp {

H265Depacketizer::H265Depacketizer() : h265_packet_(std::make_unique<H265Packet>()) {
}

H265Depacketizer::~H265Depacketizer() = default;

void H265Depacketizer::WithDONL(bool value) {
    h265_packet_->WithDONL(value);
}

bool H265Depacketizer::IsPartitionHead(const std::vector<uint8_t>& payload) {
    return h265_packet_->IsPartitionHead(payload);
}

bool H265Depacketizer::IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) {
    if (payload.size() < 3) {
        return false;
    }

    H265NALUHeader header(payload[0], payload[1]);
    if (header.Type() == kH265NaluFragmentationUnitType) {
        return H265FragmentationUnitHeader(payload[2]).E();
    }

    return marker;
}

std::vector<uint8_t> H265Depacketizer::Process(const std::vector<uint8_t>& packet) {
    std::vector<uint8_t> h265_frame;
    if (Depacketize(packet, &h265_frame)) {
        return h265_frame;
    }
    return {};
}

bool H265Depacketizer::Depacketize(const std::vector<uint8_t>& rtp_packet, std::vector<uint8_t>* h265_frame) {
    if (!h265_frame) {
        return false;
    }
    
    Packet packet;
    if (!packet.Depacketize(rtp_packet)) {
        return false;
    }

    const std::vector<uint8_t>& payload = packet.payload;
    if (!h265_packet_->Unmarshal(payload)) {
        return false;
    }

    switch (h265_packet_->GetPacketType()) {
        case H265Packet::PacketType::FragmentationUnit: {
            auto fu_packet = h265_packet_->GetFragmentationUnitPacket();
            if (!fu_packet) {
                return false;
            }

            H265FragmentationUnitHeader fu_header = fu_packet->FuHeader();
            
            if (fu_header.S()) {
                // Start of fragmented NAL unit
                if (!current_fragment_is_valid_) {
                    fragment_buffer_.clear();
                    current_fragment_is_valid_ = true;
                    
                    // Create NAL header for the first fragment
                    uint8_t f_bit = fu_packet->PayloadHeader().F() ? 1 : 0;
                    uint8_t type = fu_header.FuType();
                    uint8_t layer_id = fu_packet->PayloadHeader().LayerID();
                    uint8_t tid = fu_packet->PayloadHeader().TID();
                    
                    // Reconstruct the NAL header and add it to the buffer
                    uint16_t reconstructed_header = (f_bit << 15) | (type << 9) | (layer_id << 3) | tid;
                    fragment_buffer_.push_back(static_cast<uint8_t>(reconstructed_header >> 8));
                    fragment_buffer_.push_back(static_cast<uint8_t>(reconstructed_header & 0xFF));
                }
            } else if (!current_fragment_is_valid_) {
                // Middle or end fragment without a valid start - discard
                return false;
            }

            // Append fragment payload to buffer
            const std::vector<uint8_t>& fu_payload = fu_packet->Payload();
            fragment_buffer_.insert(fragment_buffer_.end(), fu_payload.begin(), fu_payload.end());

            // If this is the end fragment, copy the assembled NAL unit to the output
            if (fu_header.E()) {
                *h265_frame = fragment_buffer_;
                current_fragment_is_valid_ = false;
                return true;
            }
            
            // Middle fragment - don't output anything yet
            return false;
        }

        case H265Packet::PacketType::SingleNALU: {
            auto single_packet = h265_packet_->GetSingleNALUPacket();
            if (!single_packet) {
                return false;
            }

            // Construct complete NAL unit
            h265_frame->clear();
            
            // Add NAL unit header (first 2 bytes)
            uint16_t header_value = single_packet->PayloadHeader().GetValue();
            h265_frame->push_back(static_cast<uint8_t>(header_value >> 8));
            h265_frame->push_back(static_cast<uint8_t>(header_value & 0xFF));
            
            // Add payload
            const std::vector<uint8_t>& payload = single_packet->Payload();
            h265_frame->insert(h265_frame->end(), payload.begin(), payload.end());
            
            return true;
        }

        case H265Packet::PacketType::AggregationPacket: {
            auto agg_packet = h265_packet_->GetAggregationPacket();
            if (!agg_packet) {
                return false;
            }

            const H265AggregationUnitFirst* first_unit = agg_packet->FirstUnit();
            if (!first_unit) {
                return false;
            }
            
            // Process the first unit only
            h265_frame->clear();
            h265_frame->insert(h265_frame->end(), 
                              first_unit->NalUnit().begin(),
                              first_unit->NalUnit().end());
            
            return true;
        }

        case H265Packet::PacketType::PACIPacket: {
            auto paci_packet = h265_packet_->GetPACIPacket();
            if (!paci_packet) {
                return false;
            }
            
            // Extract the PACI payload
            h265_frame->clear();
            
            // Construct appropriate NAL unit header
            uint8_t type = paci_packet->CType();
            uint8_t f_bit = paci_packet->A() ? 1 : 0;
            uint8_t layer_id = 0; // Default to 0
            uint8_t tid = 0; // Default to 0
            
            uint16_t reconstructed_header = (f_bit << 15) | (type << 9) | (layer_id << 3) | tid;
            h265_frame->push_back(static_cast<uint8_t>(reconstructed_header >> 8));
            h265_frame->push_back(static_cast<uint8_t>(reconstructed_header & 0xFF));
            
            // Add payload
            const std::vector<uint8_t>& payload = paci_packet->Payload();
            h265_frame->insert(h265_frame->end(), payload.begin(), payload.end());
            
            return true;
        }

        default:
            return false;
    }
}

} // namespace rtp