#ifndef RTP_PACKET_H_
#define RTP_PACKET_H_

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <mutex>

namespace rtp {

// Constants for bitwise operations
constexpr uint8_t kVersionShift = 6;
constexpr uint8_t kVersionMask = 0x3;
constexpr uint8_t kPaddingShift = 5;
constexpr uint8_t kPaddingMask = 0x1;
constexpr uint8_t kExtensionShift = 4;
constexpr uint8_t kExtensionMask = 0x1;
constexpr uint8_t kCCMask = 0xF;
constexpr uint8_t kMarkerShift = 7;
constexpr uint8_t kMarkerMask = 0x1;
constexpr uint8_t kPayloadTypeMask = 0x7F;
constexpr uint16_t kExtensionProfileOneByte = 0xBEDE;
constexpr uint16_t kExtensionProfileTwoByte = 0x1000;
constexpr uint8_t kExtensionIDReserved = 0xF;

// Offsets within the packet
constexpr int kHeaderLength = 4;
constexpr int kSeqNumOffset = 2;
constexpr int kSeqNumLength = 2;
constexpr int kTimestampOffset = 4;
constexpr int kTimestampLength = 4;
constexpr int kSsrcOffset = 8;
constexpr int kSsrcLength = 4;
constexpr int kCsrcOffset = 12;
constexpr int kCsrcLength = 4;

// RTP Extension structure
class Extension {
public:
    Extension() = default;
    Extension(uint8_t id, const std::vector<uint8_t>& payload)
        : id_(id), payload_(payload) {}

    uint8_t id() const { return id_; }
    const std::vector<uint8_t>& payload() const { return payload_; }

private:
    uint8_t id_ = 0;
    std::vector<uint8_t> payload_;
};

// Header represents an RTP packet header
class Header {
public:
    Header();
    ~Header() = default;

    // Parsing and serialization
    bool Depacketize(const std::vector<uint8_t>& buf, size_t* bytes_read);
    bool PacketizeTo(std::vector<uint8_t>* buf) const;
    std::vector<uint8_t> Packetize() const;
    size_t PacketSize() const;

    // Extension handling
    bool SetExtension(uint8_t id, const std::vector<uint8_t>& payload);
    std::vector<uint8_t> GetExtension(uint8_t id) const;
    std::vector<uint8_t> GetExtensionIDs() const;
    bool DeleteExtension(uint8_t id);

    // Clone
    Header Clone() const;

    // RTP header fields
    uint8_t version = 2;
    bool padding = false;
    bool extension = false;
    bool marker = false;
    uint8_t payload_type = 0;
    uint16_t sequence_number = 0;
    uint32_t timestamp = 0;
    uint32_t ssrc = 0;
    std::vector<uint32_t> csrc;
    uint16_t extension_profile = 0;
    std::vector<Extension> extensions;
};

// Packet represents a complete RTP packet
class Packet {
public:
    Packet();
    ~Packet() = default;

    // Parsing and serialization
    bool Depacketize(const std::vector<uint8_t>& buf);
    std::vector<uint8_t> Packetize() const;
    bool PacketizeTo(std::vector<uint8_t>* buf) const;
    size_t PacketSize() const;

    // Clone
    std::shared_ptr<Packet> Clone() const;
    
    // String representation for debugging
    std::string ToString() const;

    // Packet components
    Header header;
    std::vector<uint8_t> payload;
    uint8_t padding_size = 0;
};

// Interface for payload processing
class PayloadProcessor {
public:
    virtual ~PayloadProcessor() = default;
    
    // Process parses the RTP payload and returns media.
    virtual std::vector<uint8_t> Process(const std::vector<uint8_t>& packet) = 0;
    
    // Checks if the packet is at the beginning of a partition.
    virtual bool IsPartitionHead(const std::vector<uint8_t>& payload) = 0;
    
    // Checks if the packet is at the end of a partition.
    virtual bool IsPartitionTail(bool marker, const std::vector<uint8_t>& payload) = 0;
};

// Sequencer generates sequential sequence numbers for building RTP packets
class Sequencer {
public:
    virtual ~Sequencer() = default;
    virtual uint16_t NextSequenceNumber() = 0;
    virtual uint64_t RollOverCount() = 0;
};

// Random sequencer implementation
class RandomSequencer : public Sequencer {
public:
    RandomSequencer();
    uint16_t NextSequenceNumber() override;
    uint64_t RollOverCount() override;

private:
    uint16_t sequence_number_;
    uint64_t roll_over_count_ = 0;
    mutable std::mutex mutex_;
};

// Fixed sequencer implementation
class FixedSequencer : public Sequencer {
public:
    explicit FixedSequencer(uint16_t starting_seq);
    uint16_t NextSequenceNumber() override;
    uint64_t RollOverCount() override;

private:
    uint16_t sequence_number_;
    uint64_t roll_over_count_ = 0;
    mutable std::mutex mutex_;
};

} // namespace rtp

#endif // RTP_PACKET_H_