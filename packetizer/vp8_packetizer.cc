#include "vp8_packetizer.h"

namespace rtp {

VP8Packetizer::VP8Packetizer(uint16_t mtu) 
    : mtu_(mtu), sequencer_(std::make_unique<RandomSequencer>()) {
}

bool VP8Packetizer::Packetize(const std::vector<uint8_t>& vp8_frame,
                            std::vector<std::vector<uint8_t>>* rtp_packets) {
    if (vp8_frame.empty() || rtp_packets == nullptr) {
        return false;
    }
    
    rtp_packets->clear();
    
    // Determine header size based on options
    uint8_t using_header_size = kVP8HeaderSize;
    if (enable_picture_id_) {
        if (picture_id_ == 0) {
            // Do nothing, use default header size
        } else if (picture_id_ < 128) {
            using_header_size = kVP8HeaderSize + 2;
        } else {
            using_header_size = kVP8HeaderSize + 3;
        }
    }
    
    // Calculate maximum fragment size
    int max_fragment_size = static_cast<int>(mtu_) - using_header_size;
    
    // Check if the payload size is valid
    int payload_data_remaining = vp8_frame.size();
    if (minValue(max_fragment_size, payload_data_remaining) <= 0) {
        return false;
    }
    
    // Fragment the VP8 frame into multiple packets
    bool first = true;
    size_t payload_data_index = 0;
    
    while (payload_data_remaining > 0) {
        int current_fragment_size = minValue(max_fragment_size, payload_data_remaining);
        
        // Create a new RTP packet
        Packet packet;
        
        // Set up the RTP header
        packet.header.ssrc = ssrc_;
        packet.header.payload_type = payload_type_;
        packet.header.sequence_number = sequencer_->NextSequenceNumber();
        packet.header.timestamp = timestamp_;
        
        // Set marker bit on the last packet
        if (payload_data_remaining <= max_fragment_size) {
            packet.header.marker = true;
        }
        
        // Prepare the VP8 payload header
        std::vector<uint8_t> payload(using_header_size + current_fragment_size);
        
        // Set up VP8 payload header
        if (first) {
            payload[0] = kVP8SBit; // Set S bit for first packet
            first = false;
        } else {
            payload[0] = 0;
        }
        
        if (enable_picture_id_) {
            switch (using_header_size) {
                case kVP8HeaderSize:
                    // No PictureID included
                    break;
                case kVP8HeaderSize + 2:
                    payload[0] |= kVP8XBit;            // Set X bit
                    payload[1] |= kVP8IBit;            // Set I bit
                    payload[2] |= uint8_t(picture_id_ & 0x7F); // 7-bit PictureID
                    break;
                case kVP8HeaderSize + 3:
                    payload[0] |= kVP8XBit;            // Set X bit
                    payload[1] |= kVP8IBit;            // Set I bit
                    payload[2] |= kVP8MBit | uint8_t((picture_id_ >> 8) & 0x7F); // M bit + high bits
                    payload[3] |= uint8_t(picture_id_ & 0xFF);  // Low 8 bits
                    break;
            }
        }
        
        // Copy VP8 frame data into the payload
        std::copy(vp8_frame.begin() + payload_data_index, 
                 vp8_frame.begin() + payload_data_index + current_fragment_size,
                 payload.begin() + using_header_size);
        
        // Set the payload in the RTP packet
        packet.payload = std::move(payload);
        
        // Packetize the RTP packet
        std::vector<uint8_t> rtp_packet = packet.Packetize();
        rtp_packets->push_back(std::move(rtp_packet));
        
        // Update counters
        payload_data_remaining -= current_fragment_size;
        payload_data_index += current_fragment_size;
    }
    
    // Update picture ID for next frame
    if (enable_picture_id_) {
        picture_id_ = (picture_id_ + 1) & 0x7FFF; // Keep within 15 bits
    }
    
    return true;
}

} // namespace rtp