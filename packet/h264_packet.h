#ifndef H264_PACKET_H_
#define H264_PACKET_H_

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <functional>  // Added this include for std::function

#include "rtp_packet.h"

namespace rtp {

// H.264 NAL unit types
constexpr uint8_t kStapaNALUType = 24;
constexpr uint8_t kFuaNALUType = 28;
constexpr uint8_t kFubNALUType = 29;
constexpr uint8_t kSpsNALUType = 7;
constexpr uint8_t kPpsNALUType = 8;
constexpr uint8_t kAudNALUType = 9;
constexpr uint8_t kFillerNALUType = 12;

// H.264 constants
constexpr uint8_t kFuaHeaderSize = 2;
constexpr uint8_t kStapaHeaderSize = 1;
constexpr uint8_t kStapaNALULengthSize = 2;

constexpr uint8_t kNaluTypeBitmask = 0x1F;
constexpr uint8_t kNaluRefIdcBitmask = 0x60;
constexpr uint8_t kFuStartBitmask = 0x80;
constexpr uint8_t kFuEndBitmask = 0x40;

constexpr uint8_t kOutputStapAHeader = 0x78;

// H.264 start codes
const std::vector<uint8_t> kNaluStartCode = {0x00, 0x00, 0x01};
const std::vector<uint8_t> kAnnexbNALUStartCode = {0x00, 0x00, 0x00, 0x01};

// Common H.264 packet functionality
class H264Packet {
public:
    H264Packet();
    virtual ~H264Packet() = default;

    // Utility functions
    static bool IsPartitionHead(const std::vector<uint8_t>& payload);
    static bool IsPartitionTail(bool marker, const std::vector<uint8_t>& payload);

protected:
    // Helper function to find NAL units in a buffer
    static void EmitNalus(const std::vector<uint8_t>& data, 
                         std::function<void(const std::vector<uint8_t>&)> emit_func);
    
    // Helper to package NALU with/without AVC format
    std::vector<uint8_t> DoPackaging(const std::vector<uint8_t>& nalu) const;

    bool is_avc_ = false;
};

// Error codes and messages
enum H264ErrorCode {
    kShortPacket,
    kUnhandledNALUType
};

std::string GetH264ErrorMessage(H264ErrorCode code);

} // namespace rtp

#endif // H264_PACKET_H_