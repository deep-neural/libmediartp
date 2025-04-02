#ifndef VP8_PACKET_H_
#define VP8_PACKET_H_

#include <cstdint>
#include <vector>
#include "rtp_packet.h"

namespace rtp {

// VP8 Header constants
constexpr uint8_t kVP8HeaderSize = 1;
constexpr uint8_t kVP8XBit = 0x80;         // 10000000
constexpr uint8_t kVP8NBit = 0x20;         // 00100000
constexpr uint8_t kVP8SBit = 0x10;         // 00010000
constexpr uint8_t kVP8PIDMask = 0x07;      // 00000111

// VP8 Extended Header constants
constexpr uint8_t kVP8IBit = 0x80;         // 10000000
constexpr uint8_t kVP8LBit = 0x40;         // 01000000
constexpr uint8_t kVP8TBit = 0x20;         // 00100000
constexpr uint8_t kVP8KBit = 0x10;         // 00010000
constexpr uint8_t kVP8MBit = 0x80;         // 10000000
constexpr uint8_t kVP8PictureIDMask = 0x7F; // 01111111

// VP8Packet represents the VP8 header that is stored in the payload of an RTP Packet
class VP8Packet {
public:
    VP8Packet() = default;
    ~VP8Packet() = default;

    // Parse VP8 packet from RTP payload
    bool Unmarshal(const std::vector<uint8_t>& payload, std::vector<uint8_t>* output);
    
    // Check if this is a head of the VP8 partition
    bool IsPartitionHead(const std::vector<uint8_t>& payload) const;

    // Required Header
    uint8_t X = 0;        // Extended control bits present
    uint8_t N = 0;        // When set to 1 this frame can be discarded
    uint8_t S = 0;        // Start of VP8 partition
    uint8_t PID = 0;      // Partition index

    // Extended control bits
    uint8_t I = 0;        // 1 if PictureID is present
    uint8_t L = 0;        // 1 if TL0PICIDX is present
    uint8_t T = 0;        // 1 if TID is present
    uint8_t K = 0;        // 1 if KEYIDX is present

    // Optional extension
    uint16_t PictureID = 0;   // 8 or 16 bits, picture ID
    uint8_t TL0PICIDX = 0;    // 8 bits temporal level zero index
    uint8_t TID = 0;          // 2 bits temporal layer index
    uint8_t Y = 0;            // 1 bit layer sync bit
    uint8_t KEYIDX = 0;       // 5 bits temporal key frame index

    std::vector<uint8_t> Payload;
};

} // namespace rtp

#endif // VP8_PACKET_H_