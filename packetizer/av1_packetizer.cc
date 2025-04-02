#include "av1_packetizer.h"
#include <algorithm>

namespace rtp {

AV1Packetizer::AV1Packetizer(size_t mtu) : mtu_(mtu) {
    // Minimum MTU for AV1 is 2 bytes (aggregation header + 1 byte)
    mtu_ = std::max<size_t>(2, mtu_);
}

bool AV1Packetizer::Packetize(const std::vector<uint8_t>& frame, 
                            std::vector<std::vector<uint8_t>>* out_packets) {
    if (out_packets == nullptr || frame.empty()) {
        return false;
    }
    
    out_packets->clear();
    
    // Parse the OBUs from the input frame
    std::vector<std::vector<uint8_t>> obu_payloads;
    size_t offset = 0;
    
    std::vector<uint8_t> current_obu_payload;
    std::unique_ptr<AV1OBUHeader::ExtensionHeader> current_packet_obu_header;
    int obus_in_packet = 0;
    bool new_sequence = false;
    bool start_with_new_packet = false;
    
    while (offset < frame.size()) {
        // Parse the OBU header
        AV1OBUHeader obu_header;
        size_t header_size;
        
        if (!AV1OBUHeader::Parse(frame, offset, &obu_header, &header_size)) {
            return false;
        }
        
        offset += header_size;
        
        // Calculate OBU size
        size_t obu_size;
        if (obu_header.has_size_field) {
            uint32_t size_value;
            size_t n;
            if (!ReadLeb128(frame, offset, &size_value, &n)) {
                return false;
            }
            
            offset += n;
            obu_size = size_value;
        } else {
            obu_size = frame.size() - offset;
        }
        
        // Check if we need to start a new packet
        bool need_new_packet = (obu_header.type == AV1OBUHeader::OBU_TEMPORAL_DELIMITER || 
                               obu_header.type == AV1OBUHeader::OBU_SEQUENCE_HEADER);
        
        // Check if we need to create a new packet due to different temporal/spatial ID
        if (!need_new_packet && obu_header.extension_header && current_packet_obu_header) {
            need_new_packet = 
                (obu_header.extension_header->spatial_id != current_packet_obu_header->spatial_id ||
                 obu_header.extension_header->temporal_id != current_packet_obu_header->temporal_id);
        }
        
        // Update current packet extension header
        if (obu_header.extension_header) {
            current_packet_obu_header = std::make_unique<AV1OBUHeader::ExtensionHeader>(
                *obu_header.extension_header);
        }
        
        if (offset + obu_size > frame.size()) {
            return false;
        }
        
        // Process the current OBU payload if we have one
        if (!current_obu_payload.empty()) {
            auto [new_payloads, new_obus_in_packet] = AppendOBUPayload(
                *out_packets,
                current_obu_payload,
                new_sequence,
                need_new_packet,
                start_with_new_packet,
                mtu_,
                obus_in_packet
            );
            
            *out_packets = std::move(new_payloads);
            obus_in_packet = new_obus_in_packet;
            current_obu_payload.clear();
            start_with_new_packet = need_new_packet;
            
            if (need_new_packet) {
                new_sequence = false;
                current_packet_obu_header.reset();
            }
        }
        
        // Skip temporal delimiter and tile list OBUs
        if (obu_header.type == AV1OBUHeader::OBU_TEMPORAL_DELIMITER || 
            obu_header.type == AV1OBUHeader::OBU_TILE_LIST) {
            offset += obu_size;
            continue;
        }
        
        // Prepare the OBU payload with non-size-field header
        current_obu_payload.resize(obu_size + header_size);
        
        // Ensure obu_has_size_field is false for RTP transport
        obu_header.has_size_field = false;
        std::vector<uint8_t> header_bytes = obu_header.Marshal();
        std::copy(header_bytes.begin(), header_bytes.end(), current_obu_payload.begin());
        std::copy(frame.begin() + offset, 
                 frame.begin() + offset + obu_size, 
                 current_obu_payload.begin() + header_size);
        
        offset += obu_size;
        new_sequence = (obu_header.type == AV1OBUHeader::OBU_SEQUENCE_HEADER);
    }
    
    // Process the last OBU payload
    if (!current_obu_payload.empty()) {
        auto [new_payloads, _] = AppendOBUPayload(
            *out_packets,
            current_obu_payload,
            new_sequence,
            true,
            start_with_new_packet,
            mtu_,
            obus_in_packet
        );
        
        *out_packets = std::move(new_payloads);
    }
    
    return true;
}

std::tuple<std::vector<std::vector<uint8_t>>, int> AV1Packetizer::AppendOBUPayload(
    std::vector<std::vector<uint8_t>> payloads,
    const std::vector<uint8_t>& obu_payload,
    bool is_new_video_sequence,
    bool is_last,
    bool start_with_new_packet,
    int mtu,
    int current_obu_count) {
    
    int current_payload = static_cast<int>(payloads.size()) - 1;
    int free_space = 0;
    
    if (current_payload >= 0) {
        free_space = mtu - static_cast<int>(payloads[current_payload].size());
    }
    
    // Create a new packet if needed
    if (current_payload < 0 || free_space <= 0 || start_with_new_packet) {
        std::vector<uint8_t> payload(1, 0);  // Start with aggregation header
        
        // Set N bit if this is a new video sequence
        if (is_new_video_sequence) {
            payload[0] |= kAV1NMask;
        }
        
        payloads.push_back(payload);
        current_payload = static_cast<int>(payloads.size() - 1);
        free_space = mtu - 1;  // Account for aggregation header
        current_obu_count = 0;
    }
    
    size_t remaining = obu_payload.size();
    size_t to_write = remaining;
    
    if (to_write > static_cast<size_t>(free_space)) {
        to_write = free_space;
    }
    
    // Decide if we should use the W field
    bool should_use_w_field = (is_last || to_write >= static_cast<size_t>(free_space)) && 
                              current_obu_count < 3;
    
    if (should_use_w_field) {
        // Set W field to number of OBUs in packet
        payloads[current_payload][0] |= static_cast<uint8_t>((current_obu_count + 1) << kAV1WBitshift) & kAV1WMask;
        
        // Append the OBU directly
        payloads[current_payload].insert(
            payloads[current_payload].end(),
            obu_payload.begin(),
            obu_payload.begin() + to_write);
        
        current_obu_count = 0;
    } else if (free_space >= 2) {
        // Need at least 2 bytes for length field + min OBU
        to_write = ComputeWriteSize(to_write, free_space);
        
        // Add length field
        auto length_field = WriteToLeb128(to_write);
        payloads[current_payload].insert(
            payloads[current_payload].end(),
            length_field.begin(), 
            length_field.end());
        
        // Add OBU data
        payloads[current_payload].insert(
            payloads[current_payload].end(),
            obu_payload.begin(),
            obu_payload.begin() + to_write);
        
        current_obu_count++;
    } else {
        // Not enough space for even 1 byte, skip this one
        to_write = 0;
    }
    
    // Handle fragmentation
    std::vector<uint8_t> remaining_obu(obu_payload.begin() + to_write, obu_payload.end());
    remaining = remaining_obu.size();
    
    while (remaining > 0) {
        // New packet with empty aggregation header
        std::vector<uint8_t> payload(1, 0);
        payloads.push_back(payload);
        current_payload++;
        
        // If we wrote something to the previous packet, set Y bit on previous and Z bit on current
        if (to_write != 0) {
            payloads[current_payload - 1][0] |= kAV1YMask;
            payloads[current_payload][0] |= kAV1ZMask;
        }
        
        to_write = remaining;
        if (to_write > mtu - 1) {  // MTU - aggregation header
            to_write = mtu - 1;
        }
        
        // For the last fragment (or full fragment), use W=1
        if (is_last || remaining <= static_cast<size_t>(mtu - 1)) {
            payloads[current_payload][0] |= 1 << kAV1WBitshift;
        } else {
            to_write = ComputeWriteSize(to_write, mtu - 1);
            auto length_field = WriteToLeb128(to_write);
            payloads[current_payload].insert(
                payloads[current_payload].end(),
                length_field.begin(), 
                length_field.end());
        }
        
        // Add fragment data
        payloads[current_payload].insert(
            payloads[current_payload].end(),
            remaining_obu.begin(),
            remaining_obu.begin() + to_write);
        
        remaining_obu.erase(remaining_obu.begin(), remaining_obu.begin() + to_write);
        remaining = remaining_obu.size();
        current_obu_count = 1;
    }
    
    return {payloads, current_obu_count};
}

size_t AV1Packetizer::ComputeWriteSize(size_t want_to_write, size_t can_write) const {
    size_t leb128_size;
    bool is_at_edge;
    
    Leb128Size(want_to_write, &leb128_size, &is_at_edge);
    
    if (can_write >= want_to_write + leb128_size) {
        return want_to_write;
    }
    
    // Handle edge case where subtracting one from the value
    // results in a smaller leb128 size that can fit
    if (is_at_edge && can_write >= want_to_write + leb128_size - 1) {
        return want_to_write - 1;
    }
    
    return want_to_write - leb128_size;
}

void AV1Packetizer::Leb128Size(size_t value, size_t* size, bool* is_at_edge) const {
    *is_at_edge = false;
    
    if (value >= 268435456) { // 2^28
        *size = 5;
        *is_at_edge = (value == 268435456);
    } else if (value >= 2097152) { // 2^21
        *size = 4;
        *is_at_edge = (value == 2097152);
    } else if (value >= 16384) { // 2^14
        *size = 3;
        *is_at_edge = (value == 16384);
    } else if (value >= 128) { // 2^7
        *size = 2;
        *is_at_edge = (value == 128);
    } else {
        *size = 1;
    }
}

} // namespace rtp