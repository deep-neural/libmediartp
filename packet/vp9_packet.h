#ifndef RTP_VP9_PACKET_H_
#define RTP_VP9_PACKET_H_

#include <cstdint>
#include <vector>
#include <array>
#include <memory>

namespace rtp {

constexpr int kMaxSpatialLayers = 5;
constexpr int kMaxVP9RefPics = 3;

// Forward declarations
class VP9Header;

// Error constants
extern const char* kErrNilPacketVP9;
extern const char* kErrShortPacketV2;
extern const char* kErrTooManySpatialLayers;
extern const char* kErrTooManyPDiff;

// VP9Packet represents the VP9 header that is stored in the payload of an RTP Packet
class VP9Packet {
public:
    VP9Packet() = default;
    ~VP9Packet() = default;

    // Parse the VP9 packet from RTP payload
    bool Unmarshal(const std::vector<uint8_t>& packet, std::vector<uint8_t>* payload);
    
    // Check if this is a head of the VP9 partition
    static bool IsPartitionHead(const std::vector<uint8_t>& payload);

    // Required header fields
    bool I = false;  // PictureID is present
    bool P = false;  // Inter-picture predicted frame
    bool L = false;  // Layer indices is present
    bool F = false;  // Flexible mode
    bool B = false;  // Start of a frame
    bool E = false;  // End of a frame
    bool V = false;  // Scalability structure (SS) data present
    bool Z = false;  // Not a reference frame for upper spatial layers

    // Recommended header fields
    uint16_t PictureID = 0;  // 7 or 16 bits, picture ID

    // Conditionally recommended header fields
    uint8_t TID = 0;  // Temporal layer ID
    bool U = false;   // Switching up point
    uint8_t SID = 0;  // Spatial layer ID
    bool D = false;   // Inter-layer dependency used

    // Conditionally required header fields
    std::vector<uint8_t> PDiff;  // Reference index (F=1)
    uint8_t TL0PICIDX = 0;       // Temporal layer zero index (F=0)

    // Scalability structure headers
    uint8_t NS = 0;               // N_S + 1 indicates the number of spatial layers present in the VP9 stream
    bool Y = false;               // Each spatial layer's frame resolution present
    bool G = false;               // PG description present flag
    uint8_t NG = 0;               // N_G indicates the number of pictures in a Picture Group (PG)
    std::vector<uint16_t> Width;  // Width of each spatial layer
    std::vector<uint16_t> Height; // Height of each spatial layer
    std::vector<uint8_t> PGTID;   // Temporal layer ID of pictures in a Picture Group
    std::vector<bool> PGU;        // Switching up point of pictures in a Picture Group
    std::vector<std::vector<uint8_t>> PGPDiff; // Reference indices of pictures in a Picture Group

private:
    int parsePictureID(const std::vector<uint8_t>& packet, int pos);
    int parseLayerInfo(const std::vector<uint8_t>& packet, int pos);
    int parseLayerInfoCommon(const std::vector<uint8_t>& packet, int pos);
    int parseLayerInfoNonFlexibleMode(const std::vector<uint8_t>& packet, int pos);
    int parseRefIndices(const std::vector<uint8_t>& packet, int pos);
    int parseSSData(const std::vector<uint8_t>& packet, int pos);
};

// VP9Header is a VP9 Frame header
class VP9Header {
public:
    VP9Header() = default;
    ~VP9Header() = default;

    // Parse the VP9 header from the payload
    bool Unmarshal(const std::vector<uint8_t>& buf);

    // Get frame dimensions
    uint16_t Width() const;
    uint16_t Height() const;

    // Header fields
    uint8_t Profile = 0;
    bool ShowExistingFrame = false;
    uint8_t FrameToShowMapIdx = 0;
    bool NonKeyFrame = false;
    bool ShowFrame = false;
    bool ErrorResilientMode = false;

    // Color config
    struct ColorConfig {
        bool TenOrTwelveBit = false;
        uint8_t BitDepth = 8;
        uint8_t ColorSpace = 0;
        bool ColorRange = false;
        bool SubsamplingX = true;
        bool SubsamplingY = true;
        
        bool Unmarshal(uint8_t profile, const std::vector<uint8_t>& buf, int* pos);
    };
    std::unique_ptr<ColorConfig> ColorConfigData;

    // Frame size
    struct FrameSize {
        uint16_t FrameWidthMinus1 = 0;
        uint16_t FrameHeightMinus1 = 0;
        
        bool Unmarshal(const std::vector<uint8_t>& buf, int* pos);
    };
    std::unique_ptr<FrameSize> FrameSizeData;

private:
    // Error constants
    static constexpr const char* kErrInvalidFrameMarker = "invalid frame marker";
    static constexpr const char* kErrWrongFrameSyncByte0 = "wrong frame_sync_byte_0";
    static constexpr const char* kErrWrongFrameSyncByte1 = "wrong frame_sync_byte_1";
    static constexpr const char* kErrWrongFrameSyncByte2 = "wrong frame_sync_byte_2";
};

// Bit reading utilities
bool hasSpace(const std::vector<uint8_t>& buf, int pos, int n);
bool readFlag(const std::vector<uint8_t>& buf, int* pos);
bool readFlagUnsafe(const std::vector<uint8_t>& buf, int* pos);
uint64_t readBits(const std::vector<uint8_t>& buf, int* pos, int n);
uint64_t readBitsUnsafe(const std::vector<uint8_t>& buf, int* pos, int n);

} // namespace rtp

#endif // RTP_VP9_PACKET_H_