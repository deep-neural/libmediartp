#ifndef RTP_AV1_PACKET_H_
#define RTP_AV1_PACKET_H_

#include <cstdint>
#include <vector>
#include <memory>

namespace rtp {

// Constants for bitwise operations in AV1 aggregation header
constexpr uint8_t kAV1ZMask = 0x80;      // 0b10000000
constexpr uint8_t kAV1ZBitshift = 7;
constexpr uint8_t kAV1YMask = 0x40;       // 0b01000000
constexpr uint8_t kAV1YBitshift = 6;
constexpr uint8_t kAV1WMask = 0x30;       // 0b00110000
constexpr uint8_t kAV1WBitshift = 4;
constexpr uint8_t kAV1NMask = 0x08;       // 0b00001000
constexpr uint8_t kAV1NBitshift = 3;

// Errors
constexpr const char* kErrNilPacket = "Nil packet";
constexpr const char* kErrShortPacket = "Packet too short";
constexpr const char* kErrIsKeyframeAndFragment = "Packet cannot be both keyframe and fragment";

// Functions to read/write LEB128 values 
// Returns the decoded value, number of bytes read, and success status
bool ReadLeb128(const std::vector<uint8_t>& input, 
                size_t offset, 
                uint32_t* value, 
                size_t* bytes_read);

// Write a value as LEB128 encoding
std::vector<uint8_t> WriteToLeb128(uint32_t value);

class AV1OBUHeader {
public:
    // OBU types
    enum Type {
        OBU_SEQUENCE_HEADER = 1,
        OBU_TEMPORAL_DELIMITER = 2,
        OBU_FRAME_HEADER = 3,
        OBU_TILE_GROUP = 4,
        OBU_METADATA = 5,
        OBU_FRAME = 6,
        OBU_REDUNDANT_FRAME_HEADER = 7,
        OBU_TILE_LIST = 8,
        OBU_PADDING = 15
    };
    
    struct ExtensionHeader {
        uint8_t temporal_id = 0;
        uint8_t spatial_id = 0;
        uint8_t reserved_3bits = 0;
        
        uint8_t Marshal() const;
    };
    
    AV1OBUHeader() = default;
    
    // Parse OBU header from data
    static bool Parse(const std::vector<uint8_t>& data, 
                      size_t offset, 
                      AV1OBUHeader* header, 
                      size_t* bytes_read);
                      
    // Serialize the OBU header
    std::vector<uint8_t> Marshal() const;
    
    // Get header size in bytes
    size_t Size() const;
    
    // OBU header fields
    Type type = OBU_SEQUENCE_HEADER;
    std::unique_ptr<ExtensionHeader> extension_header;
    bool has_size_field = false;
    bool reserved_1bit = false;
};

// AV1Packet represents a depacketized AV1 RTP Packet
class AV1Packet {
public:
    AV1Packet();
    ~AV1Packet() = default;
    
    // Unmarshal parses the passed byte vector and stores the result in this AV1Packet
    bool Unmarshal(const std::vector<uint8_t>& payload);
    
    // OBU Element access
    const std::vector<std::vector<uint8_t>>& GetOBUElements() const;

    // Access to fields
    bool GetZFlag() const { return z_; }
    bool GetYFlag() const { return y_; }
    bool GetNFlag() const { return n_; }
    uint8_t GetWValue() const { return w_; }

private:
    // Parse the packet body after the aggregation header
    bool ParseBody(const std::vector<uint8_t>& payload);

    // Z: MUST be set to 1 if the first OBU element is an
    //    OBU fragment that is a continuation of an OBU fragment
    //    from the previous packet, and MUST be set to 0 otherwise.
    bool z_ = false;

    // Y: MUST be set to 1 if the last OBU element is an OBU fragment
    //    that will continue in the next packet, and MUST be set to 0 otherwise.
    bool y_ = false;

    // W: two bit field that describes the number of OBU elements in the packet.
    uint8_t w_ = 0;

    // N: MUST be set to 1 if the packet is the first packet of a coded video sequence,
    //    and MUST be set to 0 otherwise.
    bool n_ = false;

    // Collection of OBU Elements
    std::vector<std::vector<uint8_t>> obu_elements_;
};

} // namespace rtp

#endif // RTP_AV1_PACKET_H_