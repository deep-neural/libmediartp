#ifndef RTP_VP9_PACKETIZER_H_
#define RTP_VP9_PACKETIZER_H_

#include <cstdint>
#include <vector>
#include <memory>
#include <random>
#include "rtp_packet.h"
#include "vp9_packet.h"

namespace rtp {

// VP9Payloader payloads VP9 packets
class VP9Packetizer {
public:
    explicit VP9Packetizer(uint16_t mtu = 1200);
    ~VP9Packetizer();
    
    // Payload fragments a VP9 frame across one or more RTP packets
    bool Packetize(const std::vector<uint8_t>& vp9Frame, std::vector<std::vector<uint8_t>>* rtpPackets);
    
    // Configuration
    void SetFlexibleMode(bool flexible) { flexibleMode_ = flexible; }
    void SetInitialPictureID(uint16_t id) { pictureID_ = id & 0x7FFF; }
    
private:
    std::vector<std::vector<uint8_t>> payloadFlexible(const std::vector<uint8_t>& payload);
    std::vector<std::vector<uint8_t>> payloadNonFlexible(const std::vector<uint8_t>& payload);
    
    uint16_t minInt(uint16_t a, uint16_t b) { return (a < b) ? a : b; }
    uint16_t generateRandomPictureID();
    
    uint16_t mtu_;
    bool flexibleMode_ = false;
    uint16_t pictureID_ = 0;
    bool initialized_ = false;
    
    std::unique_ptr<VP9Header> vp9Header_;
    std::mt19937 randomGenerator_;
};

} // namespace rtp

#endif // RTP_VP9_PACKETIZER_H_