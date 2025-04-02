#ifndef H264_PACKETIZER_H_
#define H264_PACKETIZER_H_

#include <cstdint>
#include <vector>
#include <memory>
#include "h264_packet.h"
#include "rtp_packet.h"

namespace rtp {

class H264Packetizer : public H264Packet {
public:
    H264Packetizer(uint16_t mtu);
    explicit H264Packetizer(uint16_t mtu, bool is_avc);
    ~H264Packetizer() override = default;

    // Packetize fragments an H.264 frame into RTP packets
    bool Packetize(const std::vector<uint8_t>& frame, std::vector<std::vector<uint8_t>>* packets);

    // EnableStapA allows combining SPS and PPS NALUs in a single packet
    void EnableStapA() { disable_stap_a_ = false; }
    void DisableStapA() { disable_stap_a_ = true; }

private:
    // Maximum size for RTP packet payload
    uint16_t mtu_;
    
    // NALUs for SPS and PPS (used for STAP-A)
    std::vector<uint8_t> sps_nalu_;
    std::vector<uint8_t> pps_nalu_;
    
    // Flag to disable STAP-A packet generation
    bool disable_stap_a_ = false;
    
    // Payload fragments an H.264 NALU into one or more RTP packets
    std::vector<std::vector<uint8_t>> Payload(const std::vector<uint8_t>& nalu);
};

} // namespace rtp

#endif // H264_PACKETIZER_H_