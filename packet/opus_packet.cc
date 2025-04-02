#include "opus_packet.h"

namespace rtp {

bool OpusPacket::Unmarshal(const std::vector<uint8_t>& packet, std::vector<uint8_t>* out_payload) {
    if (packet.empty()) {
        return false;
    }

    payload_ = packet;
    
    if (out_payload) {
        *out_payload = packet;
    }
    
    return true;
}

bool OpusPacket::IsPartitionHead(const std::vector<uint8_t>& payload) {
    // In Opus, every packet is the start of a new frame
    // This matches the behavior in the Go implementation
    return true;
}

bool OpusPacket::IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) {
    // In Opus, the marker bit indicates the end of a frame
    // Since each Opus packet is self-contained, we can simply use the marker bit
    return marker;
}

} // namespace rtp