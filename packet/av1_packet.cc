#include "av1_packet.h"
#include <stdexcept>

namespace rtp {

bool ReadLeb128(const std::vector<uint8_t>& input, 
                size_t offset, 
                uint32_t* value, 
                size_t* bytes_read) {
    if (offset >= input.size()) {
        return false;
    }
    
    *value = 0;
    *bytes_read = 0;
    uint32_t shift = 0;
    
    for (size_t i = offset; i < input.size(); i++) {
        (*bytes_read)++;
        *value |= (input[i] & 0x7f) << shift;
        
        // If MSB is not set, we're done
        if ((input[i] & 0x80) == 0) {
            return true;
        }
        
        shift += 7;
        
        // Prevent shift overflow
        if (shift >= 32) {
            return false;
        }
    }
    
    // If we get here, we ran out of bytes before finding the end of the LEB128
    return false;
}

std::vector<uint8_t> WriteToLeb128(uint32_t value) {
    std::vector<uint8_t> result;
    
    do {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        
        if (value != 0) {
            byte |= 0x80;  // Set the MSB if we have more bytes
        }
        
        result.push_back(byte);
    } while (value != 0);
    
    return result;
}

uint8_t AV1OBUHeader::ExtensionHeader::Marshal() const {
    return (temporal_id << 5) | ((spatial_id & 0x3) << 3) | (reserved_3bits & 0x07);
}

bool AV1OBUHeader::Parse(const std::vector<uint8_t>& data, 
                         size_t offset, 
                         AV1OBUHeader* header,
                         size_t* bytes_read) {
    if (offset >= data.size()) {
        return false;
    }
    
    *bytes_read = 1;  // Start with the main header byte
    
    uint8_t header_byte = data[offset];
    
    // Check forbidden bit (should be 0)
    if ((header_byte & 0x80) != 0) {
        return false;
    }
    
    header->type = static_cast<Type>((header_byte & 0x78) >> 3);
    bool extension_flag = (header_byte & 0x04) != 0;
    header->has_size_field = (header_byte & 0x02) != 0;
    header->reserved_1bit = (header_byte & 0x01) != 0;
    
    if (extension_flag) {
        if (offset + 1 >= data.size()) {
            return false;
        }
        
        header->extension_header = std::make_unique<ExtensionHeader>();
        uint8_t ext_byte = data[offset + 1];
        
        header->extension_header->temporal_id = ext_byte >> 5;
        header->extension_header->spatial_id = (ext_byte >> 3) & 0x03;
        header->extension_header->reserved_3bits = ext_byte & 0x07;
        
        (*bytes_read)++;
    }
    
    return true;
}

std::vector<uint8_t> AV1OBUHeader::Marshal() const {
    std::vector<uint8_t> result;
    result.reserve(Size());
    
    uint8_t header_byte = static_cast<uint8_t>(type << 3);
    
    if (extension_header) {
        header_byte |= 0x04;
    }
    
    if (has_size_field) {
        header_byte |= 0x02;
    }
    
    if (reserved_1bit) {
        header_byte |= 0x01;
    }
    
    result.push_back(header_byte);
    
    if (extension_header) {
        result.push_back(extension_header->Marshal());
    }
    
    return result;
}

size_t AV1OBUHeader::Size() const {
    return 1 + (extension_header ? 1 : 0);
}

AV1Packet::AV1Packet()
    : z_(false), y_(false), w_(0), n_(false) {
}

bool AV1Packet::Unmarshal(const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
        return false;
    }
    
    if (payload.size() < 2) {
        return false;
    }
    
    z_ = ((payload[0] & kAV1ZMask) >> kAV1ZBitshift) != 0;
    y_ = ((payload[0] & kAV1YMask) >> kAV1YBitshift) != 0;
    w_ = (payload[0] & kAV1WMask) >> kAV1WBitshift;
    n_ = ((payload[0] & kAV1NMask) >> kAV1NBitshift) != 0;
    
    if (z_ && n_) {
        return false;
    }
    
    return ParseBody(std::vector<uint8_t>(payload.begin() + 1, payload.end()));
}

bool AV1Packet::ParseBody(const std::vector<uint8_t>& payload) {
    obu_elements_.clear();
    
    size_t current_index = 0;
    uint32_t obu_element_length;
    size_t bytes_read;
    
    for (int i = 1; current_index < payload.size(); i++) {
        // If W bit is set the last OBU Element will have no length header
        if (static_cast<uint8_t>(i) == w_) {
            bytes_read = 0;
            obu_element_length = payload.size() - current_index;
        } else {
            if (!ReadLeb128(payload, current_index, &obu_element_length, &bytes_read)) {
                return false;
            }
        }
        
        current_index += bytes_read;
        if (current_index + obu_element_length > payload.size()) {
            return false;
        }
        
        // Copy this OBU element to our elements collection
        std::vector<uint8_t> element(payload.begin() + current_index, 
                                     payload.begin() + current_index + obu_element_length);
        obu_elements_.push_back(element);
        
        current_index += obu_element_length;
    }
    
    return true;
}

const std::vector<std::vector<uint8_t>>& AV1Packet::GetOBUElements() const {
    return obu_elements_;
}

} // namespace rtp