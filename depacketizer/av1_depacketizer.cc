#include "av1_depacketizer.h"

namespace rtp {

bool AV1Depacketizer::Depacketize(const std::vector<uint8_t>& rtp_payload, 
                                std::vector<uint8_t>* out_frame) {
    if (out_frame == nullptr) {
        return false;
    }
    
    out_frame->clear();
    
    if (rtp_payload.size() <= 1) {
        return false;
    }
    
    // Parse AV1 aggregation header
    bool obu_z = (rtp_payload[0] & kAV1ZMask) != 0;
    bool obu_y = (rtp_payload[0] & kAV1YMask) != 0;
    uint8_t obu_count = (rtp_payload[0] & kAV1WMask) >> kAV1WBitshift;
    bool obu_n = (rtp_payload[0] & kAV1NMask) != 0;
    
    // Update state
    z_ = obu_z;
    y_ = obu_y;
    n_ = obu_n;
    
    // Clear buffer on new sequence
    if (obu_n) {
        buffer_.clear();
    }
    
    // Clear buffer if this packet is not continuing a fragment
    if (!obu_z && !buffer_.empty()) {
        buffer_.clear();
    }

    size_t obu_offset = 0;
    for (size_t offset = 1; offset < rtp_payload.size(); obu_offset++) {
        bool is_first = (obu_offset == 0);
        bool is_last = (obu_count != 0 && obu_offset == static_cast<size_t>(obu_count) - 1);
        
        // Calculate OBU element length
        uint32_t length_field = 0;
        size_t n = 0;
        
        if (obu_count == 0 || !is_last) {
            if (!ReadLeb128(rtp_payload, offset, &length_field, &n)) {
                return false;
            }
            
            offset += n;
            if (obu_count == 0 && offset + length_field == rtp_payload.size()) {
                is_last = true;
            }
        } else {
            // For last element when W is non-zero, length is implicit
            length_field = rtp_payload.size() - offset;
        }
        
        if (offset + length_field > rtp_payload.size()) {
            return false;
        }
        
        std::vector<uint8_t> obu_buffer;
        
        if (is_first && obu_z) {
            // We need to combine with the previous fragment
            if (buffer_.empty()) {
                // Missing first fragment, skip this one
                if (is_last) {
                    break;
                }
                offset += length_field;
                continue;
            }
            
            // Combine with buffered fragment
            obu_buffer.reserve(buffer_.size() + length_field);
            obu_buffer.insert(obu_buffer.end(), buffer_.begin(), buffer_.end());
            obu_buffer.insert(obu_buffer.end(), 
                            rtp_payload.begin() + offset,
                            rtp_payload.begin() + offset + length_field);
            buffer_.clear();
        } else {
            // Use this OBU element directly
            obu_buffer.assign(rtp_payload.begin() + offset, 
                            rtp_payload.begin() + offset + length_field);
        }
        
        offset += length_field;
        
        // If this is the last fragment and Y is set, buffer it for next packet
        if (is_last && obu_y) {
            buffer_ = obu_buffer;
            break;
        }
        
        if (obu_buffer.empty()) {
            continue;
        }
        
        // Parse OBU header to check OBU type
        AV1OBUHeader obu_header;
        size_t header_size = 0;
        if (!AV1OBUHeader::Parse(obu_buffer, 0, &obu_header, &header_size)) {
            return false;
        }
        
        // Skip temporal delimiter and tile list OBUs
        if (obu_header.type == AV1OBUHeader::OBU_TEMPORAL_DELIMITER || 
            obu_header.type == AV1OBUHeader::OBU_TILE_LIST) {
            continue;
        }
        
        // Add OBU with size field to the output
        // OBUs in RTP should have has_size_field=0, but we add it for the output stream
        std::vector<uint8_t> obu_with_size;
        
        // Set has_size_field=true for output
        obu_header.has_size_field = true;
        auto header_bytes = obu_header.Marshal();
        
        // Calculate size of payload (without header)
        size_t payload_size = obu_buffer.size() - header_size;
        
        // Write header + size + payload
        obu_with_size.insert(obu_with_size.end(), header_bytes.begin(), header_bytes.end());
        auto size_bytes = WriteToLeb128(payload_size);
        obu_with_size.insert(obu_with_size.end(), size_bytes.begin(), size_bytes.end());
        obu_with_size.insert(obu_with_size.end(), 
                           obu_buffer.begin() + header_size,
                           obu_buffer.end());
        
        out_frame->insert(out_frame->end(), obu_with_size.begin(), obu_with_size.end());
        
        if (is_last) {
            break;
        }
    }
    
    // Validate that we processed the expected number of OBUs
    if (obu_count != 0 && obu_offset != static_cast<size_t>(obu_count) - 1) {
        return false;
    }
    
    return true;
}

bool AV1Depacketizer::IsPartitionHead(const std::vector<uint8_t>& rtp_payload) const {
    if (rtp_payload.empty()) {
        return false;
    }
    
    return (rtp_payload[0] & kAV1ZMask) == 0;
}

} // namespace rtp