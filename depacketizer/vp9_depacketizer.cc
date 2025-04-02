#include "vp9_depacketizer.h"

namespace rtp {

VP9Depacketizer::VP9Depacketizer() 
    : vp9Packet_(std::make_unique<VP9Packet>()),
      rtpPacket_(std::make_unique<Packet>()) {
}

VP9Depacketizer::~VP9Depacketizer() = default;

bool VP9Depacketizer::Depacketize(const std::vector<uint8_t>& rtpPacket, std::vector<uint8_t>* vp9Frame) {
    if (rtpPacket.empty() || !vp9Frame) {
        return false;
    }
    
    // Depacketize RTP packet
    if (!rtpPacket_->Depacketize(rtpPacket)) {
        return false;
    }
    
    // Parse VP9 specific payload
    std::vector<uint8_t> payload;
    if (!vp9Packet_->Unmarshal(rtpPacket_->payload, &payload)) {
        return false;
    }
    
    // Append to the VP9 frame
    vp9Frame->insert(vp9Frame->end(), payload.begin(), payload.end());
    
    return true;
}

bool VP9Depacketizer::IsPartitionHead(const std::vector<uint8_t>& payload) {
    return VP9Packet::IsPartitionHead(payload);
}

bool VP9Depacketizer::IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
        return false;
    }
    
    // E bit indicates the end of a VP9 partition
    return (payload[0] & 0x04) != 0 || marker;
}

} // namespace rtp