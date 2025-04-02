#ifndef H265_DEPACKETIZER_H_
#define H265_DEPACKETIZER_H_

#include "rtp_packet.h"
#include "h265_packet.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace rtp {

class H265Depacketizer : public PayloadProcessor {
public:
    H265Depacketizer();
    ~H265Depacketizer();
    
    // PayloadProcessor interface implementation
    std::vector<uint8_t> Process(const std::vector<uint8_t>& packet) override;
    bool IsPartitionHead(const std::vector<uint8_t>& payload) override;
    bool IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) override;
    
    // Depacketize a single RTP packet
    bool Depacketize(const std::vector<uint8_t>& rtp_packet, std::vector<uint8_t>* h265_frame);
    
    // Configure DONL settings
    void WithDONL(bool value);
    
private:
    std::unique_ptr<H265Packet> h265_packet_;
    std::vector<uint8_t> fragment_buffer_;
    bool current_fragment_is_valid_ = false;
};

} // namespace rtp

#endif // H265_DEPACKETIZER_H_