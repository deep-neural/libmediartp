#ifndef RTP_AV1_DEPACKETIZER_H_
#define RTP_AV1_DEPACKETIZER_H_

#include "av1_packet.h"
#include <cstdint>
#include <vector>

namespace rtp {

// AV1Depacketizer depacketizes AV1 RTP packets into AV1 OBU streams
class AV1Depacketizer {
public:
    AV1Depacketizer() = default;
    ~AV1Depacketizer() = default;
    
    // Depacketize processes an RTP packet and returns a complete AV1 frame if available
    // Returns true if successful, false on error
    // If the frame is incomplete, out_frame will be empty but the function still returns true
    bool Depacketize(const std::vector<uint8_t>& rtp_payload, 
                     std::vector<uint8_t>* out_frame);
    
    // Returns true if the RTP packet is the start of a new frame
    bool IsPartitionHead(const std::vector<uint8_t>& rtp_payload) const;

private:
    // Buffer for fragmented OBU from previous packet
    std::vector<uint8_t> buffer_;
    
    // State flags from last packet
    bool z_ = false;
    bool y_ = false;
    bool n_ = false;
};

} // namespace rtp

#endif // RTP_AV1_DEPACKETIZER_H_