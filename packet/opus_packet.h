#ifndef OPUS_PACKET_H_
#define OPUS_PACKET_H_

#include "rtp_packet.h"
#include <cstdint>
#include <vector>

namespace rtp {

// OpusPacket represents the Opus header that is stored in the payload of an RTP Packet.
class OpusPacket {
public:
    OpusPacket() = default;
    ~OpusPacket() = default;

    // Unmarshal parses the passed byte vector and stores the result in the OpusPacket.
    // Returns the parsed payload if successful.
    bool Unmarshal(const std::vector<uint8_t>& packet, std::vector<uint8_t>* out_payload);

    // IsPartitionHead checks whether if this is a head of the Opus partition.
    // Opus frames are always complete frames and are always partition heads.
    static bool IsPartitionHead(const std::vector<uint8_t>& payload);

    // IsPartitionTail checks whether if this is a tail of the Opus partition.
    // For Opus, all packets are self-contained, so the marker bit indicates partition tail.
    static bool IsPartitionTail(bool marker, const std::vector<uint8_t>& payload);

    // Access the payload
    const std::vector<uint8_t>& Payload() const { return payload_; }

private:
    std::vector<uint8_t> payload_;
};

} // namespace rtp

#endif // OPUS_PACKET_H_