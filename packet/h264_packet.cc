#include "h264_packet.h"
#include <algorithm>
#include <arpa/inet.h>

namespace rtp {

H264Packet::H264Packet() = default;

bool H264Packet::IsPartitionHead(const std::vector<uint8_t>& payload) {
    if (payload.size() < 2) {
        return false;
    }

    uint8_t nalu_type = payload[0] & kNaluTypeBitmask;
    if (nalu_type == kFuaNALUType || nalu_type == kFubNALUType) {
        return (payload[1] & kFuStartBitmask) != 0;
    }

    return true;
}

bool H264Packet::IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) {
    if (payload.size() < 2) {
        return false;
    }

    uint8_t nalu_type = payload[0] & kNaluTypeBitmask;
    if (nalu_type == kFuaNALUType || nalu_type == kFubNALUType) {
        return (payload[1] & kFuEndBitmask) != 0;
    }

    return marker;
}

void H264Packet::EmitNalus(const std::vector<uint8_t>& data, 
                         std::function<void(const std::vector<uint8_t>&)> emit_func) {
    // Look for 3-byte NALU start code
    size_t start = 0;
    size_t offset = kNaluStartCode.size();
    size_t length = data.size();

    auto find_subsequence = [](auto begin, auto end, auto s_begin, auto s_end) {
        auto it = std::search(begin, end, s_begin, s_end);
        if (it != end) {
            return std::distance(begin, it);
        }
        return std::distance(begin, end); // Changed -1 to return consistent type
    };

    auto start_pos = find_subsequence(data.begin(), data.end(), 
                                    kNaluStartCode.begin(), kNaluStartCode.end());
    
    if (start_pos == std::distance(data.begin(), data.end())) { // Changed comparison
        // No start code, emit the whole buffer
        emit_func(data);
        return;
    }

    start = start_pos;

    while (start < length) {
        // Look for the next NALU start (end of this NALU)
        auto end_pos = std::distance(data.begin(), data.end()); // Changed initialization
        if (start + offset < length) {
            end_pos = find_subsequence(data.begin() + start + offset, data.end(),
                                      kNaluStartCode.begin(), kNaluStartCode.end());
        }

        if (end_pos == std::distance(data.begin() + start + offset, data.end())) { // Changed comparison
            // No more NALUs, emit the rest of the buffer
            std::vector<uint8_t> nalu(data.begin() + start + offset, data.end());
            emit_func(nalu);
            break;
        }

        // Next NALU start
        size_t next_start = start + offset + end_pos;

        // Check if the next NALU is actually a 4-byte start code
        bool end_is_4_byte = (next_start > 0 && data[next_start - 1] == 0);
        if (end_is_4_byte) {
            next_start--;
        }

        std::vector<uint8_t> nalu(data.begin() + start + offset, data.begin() + next_start);
        emit_func(nalu);

        start = next_start;

        if (end_is_4_byte) {
            offset = 4;
        } else {
            offset = 3;
        }
    }
}

std::vector<uint8_t> H264Packet::DoPackaging(const std::vector<uint8_t>& nalu) const {
    std::vector<uint8_t> result;
    
    if (is_avc_) {
        // Add AVC length prefix (4 bytes)
        uint32_t nalu_size = htonl(static_cast<uint32_t>(nalu.size()));
        const uint8_t* size_bytes = reinterpret_cast<const uint8_t*>(&nalu_size);
        result.insert(result.end(), size_bytes, size_bytes + 4);
        result.insert(result.end(), nalu.begin(), nalu.end());
    } else {
        // Add Annex B start code
        result.insert(result.end(), kAnnexbNALUStartCode.begin(), kAnnexbNALUStartCode.end());
        result.insert(result.end(), nalu.begin(), nalu.end());
    }
    
    return result;
}

std::string GetH264ErrorMessage(H264ErrorCode code) {
    switch (code) {
        case kShortPacket:
            return "H264 packet too short";
        case kUnhandledNALUType:
            return "Unhandled NALU type";
        default:
            return "Unknown H264 error";
    }
}

} // namespace rtp