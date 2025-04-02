#include "opus_depacketizer.h"

namespace rtp {

std::vector<uint8_t> OPUSDepacketizer::Process(const std::vector<uint8_t>& packet) {
    std::vector<uint8_t> opus_payload;
    Depacketize(packet, &opus_payload);
    return opus_payload;
}

bool OPUSDepacketizer::IsPartitionHead(const std::vector<uint8_t>& payload) {
    return OpusPacket::IsPartitionHead(payload);
}

bool OPUSDepacketizer::IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) {
    return OpusPacket::IsPartitionTail(marker, payload);
}

bool OPUSDepacketizer::Depacketize(const std::vector<uint8_t>& rtp_packet, std::vector<uint8_t>* opus_frame) {
    if (rtp_packet.empty() || !opus_frame) {
        return false;
    }

    // Parse the RTP packet
    Packet packet;
    if (!packet.Depacketize(rtp_packet)) {
        return false;
    }

    // Extract the Opus payload
    OpusPacket opus_packet;
    return opus_packet.Unmarshal(packet.payload, opus_frame);
}

} // namespace rtp