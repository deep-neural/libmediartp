#ifndef RTP_VP9_DEPACKETIZER_H_
#define RTP_VP9_DEPACKETIZER_H_

#include <cstdint>
#include <vector>
#include <memory>
#include "vp9_packet.h"
#include "rtp_packet.h"

namespace rtp {

// VP9Depacketizer depacketizes VP9 RTP packets
class VP9Depacketizer {
public:
    VP9Depacketizer();
    ~VP9Depacketizer();
    
    // Process depacketizes the RTP payload and returns VP9 frame
    bool Depacketize(const std::vector<uint8_t>& rtpPacket, std::vector<uint8_t>* vp9Frame);
    
    // Checks if the packet contains the beginning of a VP9 partition
    bool IsPartitionHead(const std::vector<uint8_t>& payload);
    
    // Checks if the packet contains the end of a VP9 partition
    bool IsPartitionTail(bool marker, const std::vector<uint8_t>& payload);

private:
    std::unique_ptr<VP9Packet> vp9Packet_;
    std::unique_ptr<Packet> rtpPacket_;
};

} // namespace rtp

#endif // RTP_VP9_DEPACKETIZER_H_