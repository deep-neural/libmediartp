#include "vp8_packet.h"
#include <stdexcept>
#include <string>

namespace rtp {

bool VP8Packet::Unmarshal(const std::vector<uint8_t>& payload, std::vector<uint8_t>* output) {
    if (payload.empty()) {
        return false;
    }

    size_t payload_len = payload.size();
    size_t payload_index = 0;

    if (payload_index >= payload_len) {
        return false;
    }

    // Parse required header
    X = (payload[payload_index] & kVP8XBit) >> 7;
    N = (payload[payload_index] & kVP8NBit) >> 5;
    S = (payload[payload_index] & kVP8SBit) >> 4;
    PID = payload[payload_index] & kVP8PIDMask;

    payload_index++;

    // Parse extended control bits if present
    if (X == 1) {
        if (payload_index >= payload_len) {
            return false;
        }
        I = (payload[payload_index] & kVP8IBit) >> 7;
        L = (payload[payload_index] & kVP8LBit) >> 6;
        T = (payload[payload_index] & kVP8TBit) >> 5;
        K = (payload[payload_index] & kVP8KBit) >> 4;
        payload_index++;
    } else {
        I = 0;
        L = 0;
        T = 0;
        K = 0;
    }

    // Parse PictureID if present
    if (I == 1) {
        if (payload_index >= payload_len) {
            return false;
        }
        if (payload[payload_index] & kVP8MBit) {
            // M == 1, PID is 16bit
            if (payload_index + 1 >= payload_len) {
                return false;
            }
            PictureID = (uint16_t(payload[payload_index] & kVP8PictureIDMask) << 8) | 
                        uint16_t(payload[payload_index + 1]);
            payload_index += 2;
        } else {
            PictureID = uint16_t(payload[payload_index]);
            payload_index++;
        }
    } else {
        PictureID = 0;
    }

    // Parse TL0PICIDX if present
    if (L == 1) {
        if (payload_index >= payload_len) {
            return false;
        }
        TL0PICIDX = payload[payload_index];
        payload_index++;
    } else {
        TL0PICIDX = 0;
    }

    // Parse TID and KEYIDX if present
    if (T == 1 || K == 1) {
        if (payload_index >= payload_len) {
            return false;
        }
        if (T == 1) {
            TID = payload[payload_index] >> 6;
            Y = (payload[payload_index] >> 5) & 0x1;
        } else {
            TID = 0;
            Y = 0;
        }
        if (K == 1) {
            KEYIDX = payload[payload_index] & 0x1F;
        } else {
            KEYIDX = 0;
        }
        payload_index++;
    } else {
        TID = 0;
        Y = 0;
        KEYIDX = 0;
    }

    // Extract payload
    if (payload_index < payload_len) {
        Payload.assign(payload.begin() + payload_index, payload.end());
        if (output) {
            *output = Payload;
        }
    } else {
        Payload.clear();
        if (output) {
            output->clear();
        }
    }

    return true;
}

bool VP8Packet::IsPartitionHead(const std::vector<uint8_t>& payload) const {
    if (payload.size() < 1) {
        return false;
    }
    
    return (payload[0] & kVP8SBit) != 0;
}

} // namespace rtp