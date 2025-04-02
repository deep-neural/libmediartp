#ifndef VP8_DEPACKETIZER_H_
#define VP8_DEPACKETIZER_H_

#include <cstdint>
#include <vector>
#include "rtp_packet.h"
#include "vp8_packet.h"

namespace rtp {

// VP8Depacketizer unpacks a VP8 payload from an RTP packet
class VP8Depacketizer : public PayloadProcessor {
public:
    VP8Depacketizer() = default;
    ~VP8Depacketizer() override = default;
    
    // Process unpacks the RTP payload and returns the VP8 frame
    std::vector<uint8_t> Process(const std::vector<uint8_t>& packet) override;
    
    // Checks if the packet is at the beginning of a VP8 partition
    bool IsPartitionHead(const std::vector<uint8_t>& payload) override;
    
    // Checks if the packet is at the end of a VP8 partition
    bool IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) override;
    
    // Main depacketization function that unpacks RTP packet to VP8 frame
    bool Depacketize(const std::vector<uint8_t>& rtp_packet, std::vector<uint8_t>* vp8_frame);

private:
    VP8Packet vp8_packet_;
};

} // namespace rtp

#endif // VP8_DEPACKETIZER_H_