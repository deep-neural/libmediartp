#include "vp8_depacketizer.h"

namespace rtp {

std::vector<uint8_t> VP8Depacketizer::Process(const std::vector<uint8_t>& packet) {
    std::vector<uint8_t> vp8_frame;
    if (!vp8_packet_.Unmarshal(packet, &vp8_frame)) {
        return {};
    }
    return vp8_frame;
}

bool VP8Depacketizer::IsPartitionHead(const std::vector<uint8_t>& payload) {
    return vp8_packet_.IsPartitionHead(payload);
}

bool VP8Depacketizer::IsPartitionTail(bool marker, const std::vector<uint8_t>& /*payload*/) {
    // In VP8, a partition tail is indicated by the RTP marker bit being set
    return marker;
}

bool VP8Depacketizer::Depacketize(const std::vector<uint8_t>& rtp_packet, std::vector<uint8_t>* vp8_frame) {
    if (vp8_frame == nullptr) {
        return false;
    }
    
    // Parse the RTP packet
    Packet packet;
    if (!packet.Depacketize(rtp_packet)) {
        return false;
    }
    
    // Process the payload to extract VP8 frame data
    std::vector<uint8_t> frame_data = Process(packet.payload);
    if (frame_data.empty()) {
        return false;
    }
    
    *vp8_frame = std::move(frame_data);
    return true;
}

} // namespace rtp