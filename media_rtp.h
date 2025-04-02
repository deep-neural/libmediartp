#ifndef MEDIA_RTP_H_
#define MEDIA_RTP_H_

#include <cstdint>
#include <vector>
#include <memory>

namespace media {

// Forward declarations for internal implementation classes
namespace internal {
    class PacketizerImpl;
    class DepacketizerImpl;
}

// Supported codecs
enum class Codec {
    AV1,
    H264,
    H265,
    OPUS,
    VP8,
    VP9
};

/**
 * RTPPacketizer - Packetizes codec frames into RTP packets
 */
class RTPPacketizer {
public:
    // Constructor with codec and MTU size (default 1200 bytes)
    explicit RTPPacketizer(Codec codec, uint16_t mtu = 1200);
    ~RTPPacketizer();

    // Packetize a frame into RTP packets
    // Returns true if successful, false otherwise
    bool Packetize(const std::vector<uint8_t>& frame, 
                   std::vector<std::vector<uint8_t>>* rtp_packets);

    // Configuration methods
    void SetSSRC(uint32_t ssrc);
    void SetPayloadType(uint8_t payload_type);
    void SetTimestamp(uint32_t timestamp);
    
    // Codec-specific configuration
    
    // H264/H265 options
    void EnableStapA(bool enable); // H264-specific: combine SPS and PPS in STAP-A packets
    void SetDONL(bool enable);     // H265-specific: enable Decoding Order Number

    // VP8/VP9 options
    void EnablePictureID(bool enable);     // VP8-specific
    void SetInitialPictureID(uint16_t id); // VP9-specific
    void SetFlexibleMode(bool enable);     // VP9-specific

private:
    std::unique_ptr<internal::PacketizerImpl> impl_;
};

/**
 * RTPDepacketizer - Depacketizes RTP packets into codec frames
 */
class RTPDepacketizer {
public:
    explicit RTPDepacketizer(Codec codec);
    ~RTPDepacketizer();

    // Depacketize an RTP packet and get the frame when complete
    // Returns true if successful, false on error
    // If the frame is incomplete, out_frame will be empty but the function returns true
    bool Depacketize(const std::vector<uint8_t>& rtp_packet, 
                     std::vector<uint8_t>* out_frame);

    // Returns true if this packet is the start of a new frame
    bool IsFrameStart(const std::vector<uint8_t>& rtp_packet);

    // Returns true if this packet is the end of a frame
    bool IsFrameEnd(const std::vector<uint8_t>& rtp_packet);

    // Codec-specific configuration
    void SetDONL(bool enable); // H265-specific: Decoding Order Number present

private:
    std::unique_ptr<internal::DepacketizerImpl> impl_;
};

// Library version information
struct Version {
    int major;
    int minor;
    int patch;
};

// Get library version information
Version GetVersion();

} // namespace 

#endif