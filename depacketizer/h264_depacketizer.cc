#include "h264_depacketizer.h"
#include <stdexcept>
#include <arpa/inet.h>

namespace rtp {

H264Depacketizer::H264Depacketizer() : H264Packet() {}

H264Depacketizer::H264Depacketizer(bool is_avc) : H264Packet() {
    is_avc_ = is_avc;
}

std::vector<uint8_t> H264Depacketizer::Process(const std::vector<uint8_t>& packet) {
    Packet rtp_packet;
    if (!rtp_packet.Depacketize(packet)) {
        return {};
    }
    
    return ParseBody(rtp_packet.payload);
}

bool H264Depacketizer::Depacketize(const std::vector<uint8_t>& rtp_packet, std::vector<uint8_t>* frame) {
    if (!frame) {
        return false;
    }
    
    Packet packet;
    if (!packet.Depacketize(rtp_packet)) {
        return false;
    }
    
    std::vector<uint8_t> result = ParseBody(packet.payload);
    if (result.empty() && !packet.payload.empty()) {
        // Still in progress for fragmented packets
        return true;
    }
    
    frame->insert(frame->end(), result.begin(), result.end());
    return true;
}

bool H264Depacketizer::IsPartitionHead(const std::vector<uint8_t>& payload) {
    return H264Packet::IsPartitionHead(payload);
}

bool H264Depacketizer::IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) {
    return H264Packet::IsPartitionTail(marker, payload);
}

std::vector<uint8_t> H264Depacketizer::ParseBody(const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
        throw std::runtime_error(GetH264ErrorMessage(kShortPacket));
    }

    // Get NALU type
    uint8_t nalu_type = payload[0] & kNaluTypeBitmask;

    // Handle different NALU types
    if (nalu_type > 0 && nalu_type < 24) {
        // Single NALU
        return DoPackaging(payload);
    } else if (nalu_type == kStapaNALUType) {
        // STAP-A (Single-time aggregation packet)
        size_t curr_offset = kStapaHeaderSize;
        std::vector<uint8_t> result;

        while (curr_offset < payload.size()) {
            // Check we have enough bytes for the NALU length
            if (curr_offset + kStapaNALULengthSize > payload.size()) {
                break;
            }

            // Read NALU size (2 bytes, network byte order)
            uint16_t nalu_size = (payload[curr_offset] << 8) | payload[curr_offset + 1];
            curr_offset += kStapaNALULengthSize;

            // Check if we have enough bytes for the NALU
            if (curr_offset + nalu_size > payload.size()) {
                throw std::runtime_error(GetH264ErrorMessage(kShortPacket) + 
                                       ": STAP-A declared size larger than buffer");
            }

            // Extract the NALU
            std::vector<uint8_t> nalu(payload.begin() + curr_offset, 
                                     payload.begin() + curr_offset + nalu_size);
            
            // Package the NALU
            std::vector<uint8_t> packaged = DoPackaging(nalu);
            result.insert(result.end(), packaged.begin(), packaged.end());
            
            curr_offset += nalu_size;
        }

        return result;
    } else if (nalu_type == kFuaNALUType) {
        // FU-A (Fragmentation unit)
        if (payload.size() < kFuaHeaderSize) {
            throw std::runtime_error(GetH264ErrorMessage(kShortPacket));
        }

        // Extract the data part of the fragment
        std::vector<uint8_t> fragment(payload.begin() + kFuaHeaderSize, payload.end());
        fua_buffer_.insert(fua_buffer_.end(), fragment.begin(), fragment.end());

        // Check if this is the end of a fragmentation unit
        if (payload[1] & kFuEndBitmask) {
            // Reconstruct the original NALU
            uint8_t nalu_ref_idc = payload[0] & kNaluRefIdcBitmask;
            uint8_t fragment_type = payload[1] & kNaluTypeBitmask;
            
            std::vector<uint8_t> nalu = {nalu_ref_idc | fragment_type};
            nalu.insert(nalu.end(), fua_buffer_.begin(), fua_buffer_.end());
            
            // Clear the buffer for the next fragmented NALU
            fua_buffer_.clear();
            
            return DoPackaging(nalu);
        }
        
        // Still in progress for this fragmented NALU
        return {};
    }

    // Unhandled NALU type
    throw std::runtime_error(GetH264ErrorMessage(kUnhandledNALUType) + 
                           ": " + std::to_string(nalu_type));
}

} // namespace rtp