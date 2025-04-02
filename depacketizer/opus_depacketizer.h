#ifndef OPUS_DEPACKETIZER_H_
#define OPUS_DEPACKETIZER_H_

#include "opus_packet.h"
#include "rtp_packet.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace rtp {

class OPUSDepacketizer : public PayloadProcessor {
public:
    OPUSDepacketizer() = default;
    ~OPUSDepacketizer() override = default;

    // Process implements the PayloadProcessor interface.
    // It extracts the Opus payload from an RTP packet.
    std::vector<uint8_t> Process(const std::vector<uint8_t>& packet) override;

    // IsPartitionHead implements the PayloadProcessor interface.
    // It checks if the packet is at the beginning of a partition.
    bool IsPartitionHead(const std::vector<uint8_t>& payload) override;

    // IsPartitionTail implements the PayloadProcessor interface.
    // It checks if the packet is at the end of a partition.
    bool IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) override;

    // Depacketizer extracts the Opus payload from an RTP packet.
    bool Depacketize(const std::vector<uint8_t>& rtp_packet, std::vector<uint8_t>* opus_frame);
};

} // namespace rtp

#endif // OPUS_DEPACKETIZER_H_