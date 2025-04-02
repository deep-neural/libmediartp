#ifndef H264_DEPACKETIZER_H_
#define H264_DEPACKETIZER_H_

#include <cstdint>
#include <vector>
#include <memory>
#include "h264_packet.h"
#include "rtp_packet.h"

namespace rtp {

class H264Depacketizer : public H264Packet, public PayloadProcessor {
public:
    H264Depacketizer();
    explicit H264Depacketizer(bool is_avc);
    ~H264Depacketizer() override = default;

    // Process parses the RTP payload and returns H.264 media
    std::vector<uint8_t> Process(const std::vector<uint8_t>& packet) override;

    // Depacketize parses the passed RTP packet and stores the result
    bool Depacketize(const std::vector<uint8_t>& rtp_packet, std::vector<uint8_t>* frame);

    // IsPartitionHead checks if this is the head of an H264 partition
    bool IsPartitionHead(const std::vector<uint8_t>& payload) override;
    
    // IsPartitionTail checks if this is the tail of an H264 partition
    bool IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) override;

private:
    // Parse the H.264 payload
    std::vector<uint8_t> ParseBody(const std::vector<uint8_t>& payload);
    
    // Buffer for assembling fragmented NALUs
    std::vector<uint8_t> fua_buffer_;
};

} // namespace rtp

#endif // H264_DEPACKETIZER_H_