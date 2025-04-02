#ifndef H265_PACKET_H_
#define H265_PACKET_H_

#include <cstdint>
#include <vector>
#include <memory>

namespace rtp {

// H265 NAL Unit Header
// +---------------+---------------+
// |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |F|   Type    |  LayerID  | TID |
// +-------------+-----------------+
class H265NALUHeader {
public:
    H265NALUHeader() = default;
    explicit H265NALUHeader(uint16_t value);
    H265NALUHeader(uint8_t highByte, uint8_t lowByte);

    // Getters for header fields
    bool F() const;              // Forbidden bit
    uint8_t Type() const;        // NAL unit type
    bool IsTypeVCLUnit() const;  // Check if NAL unit is VCL
    uint8_t LayerID() const;     // Layer ID
    uint8_t TID() const;         // Temporal identifier

    // Packet type checks
    bool IsAggregationPacket() const;
    bool IsFragmentationUnit() const;
    bool IsPACIPacket() const;

    uint16_t GetValue() const { return value_; }

private:
    uint16_t value_ = 0;
};

// H265 Fragmentation Unit Header
// +---------------+
// |0|1|2|3|4|5|6|7|
// +-+-+-+-+-+-+-+-+
// |S|E|  FuType   |
// +---------------+
class H265FragmentationUnitHeader {
public:
    H265FragmentationUnitHeader() = default;
    explicit H265FragmentationUnitHeader(uint8_t value);

    bool S() const;          // Start bit
    bool E() const;          // End bit
    uint8_t FuType() const;  // FU type

    uint8_t GetValue() const { return value_; }

private:
    uint8_t value_ = 0;
};

// H265TSCI - Temporal Scalability Control Information
class H265TSCI {
public:
    H265TSCI() = default;
    explicit H265TSCI(uint32_t value);

    uint8_t TL0PICIDX() const;
    uint8_t IrapPicID() const;
    bool S() const;
    bool E() const;
    uint8_t RES() const;

    uint32_t GetValue() const { return value_; }

private:
    uint32_t value_ = 0;
};

// Base class for H265 packets
class H265PacketBase {
public:
    virtual ~H265PacketBase() = default;
    virtual bool Unmarshal(const std::vector<uint8_t>& payload) = 0;
};

// H265 Single NAL Unit Packet
class H265SingleNALUnitPacket : public H265PacketBase {
public:
    H265SingleNALUnitPacket();
    bool Unmarshal(const std::vector<uint8_t>& payload) override;
    void WithDONL(bool value);

    H265NALUHeader PayloadHeader() const;
    const uint16_t* DONL() const;
    const std::vector<uint8_t>& Payload() const;

private:
    H265NALUHeader payload_header_;
    std::unique_ptr<uint16_t> donl_;
    std::vector<uint8_t> payload_;
    bool might_need_donl_ = false;
};

// H265 Aggregation Unit (First unit in an Aggregation Packet)
class H265AggregationUnitFirst {
public:
    H265AggregationUnitFirst();

    const uint16_t* DONL() const;
    uint16_t NALUSize() const;
    const std::vector<uint8_t>& NalUnit() const;

    std::unique_ptr<uint16_t> donl_;
    uint16_t nalu_size_ = 0;
    std::vector<uint8_t> nal_unit_;
};

// H265 Aggregation Unit (Other units)
class H265AggregationUnit {
public:
    H265AggregationUnit();

    const uint8_t* DOND() const;
    uint16_t NALUSize() const;
    const std::vector<uint8_t>& NalUnit() const;

    std::unique_ptr<uint8_t> dond_;
    uint16_t nalu_size_ = 0;
    std::vector<uint8_t> nal_unit_;
};

// H265 Aggregation Packet
class H265AggregationPacket : public H265PacketBase {
public:
    H265AggregationPacket();
    bool Unmarshal(const std::vector<uint8_t>& payload) override;
    void WithDONL(bool value);

    const H265AggregationUnitFirst* FirstUnit() const;
    const std::vector<H265AggregationUnit>& OtherUnits() const;

private:
    std::unique_ptr<H265AggregationUnitFirst> first_unit_;
    std::vector<H265AggregationUnit> other_units_;
    bool might_need_donl_ = false;
};

// H265 Fragmentation Unit Packet
class H265FragmentationUnitPacket : public H265PacketBase {
public:
    H265FragmentationUnitPacket();
    bool Unmarshal(const std::vector<uint8_t>& payload) override;
    void WithDONL(bool value);

    H265NALUHeader PayloadHeader() const;
    H265FragmentationUnitHeader FuHeader() const;
    const uint16_t* DONL() const;
    const std::vector<uint8_t>& Payload() const;

private:
    H265NALUHeader payload_header_;
    H265FragmentationUnitHeader fu_header_;
    std::unique_ptr<uint16_t> donl_;
    std::vector<uint8_t> payload_;
    bool might_need_donl_ = false;
};

// H265 PACI Packet
class H265PACIPacket : public H265PacketBase {
public:
    H265PACIPacket();
    bool Unmarshal(const std::vector<uint8_t>& payload) override;

    H265NALUHeader PayloadHeader() const;
    bool A() const;
    uint8_t CType() const;
    uint8_t PHSsize() const;
    bool F0() const;
    bool F1() const;
    bool F2() const;
    bool Y() const;
    const std::vector<uint8_t>& PHES() const;
    const std::vector<uint8_t>& Payload() const;
    const H265TSCI* TSCI() const;

private:
    H265NALUHeader payload_header_;
    uint16_t paci_header_fields_ = 0;
    std::vector<uint8_t> phes_;
    std::vector<uint8_t> payload_;
    mutable std::unique_ptr<H265TSCI> tsci_;
};

// H265 Packet - Main container for H.265 packet data
class H265Packet {
public:
    H265Packet();
    ~H265Packet() = default;

    bool Unmarshal(const std::vector<uint8_t>& payload);
    void WithDONL(bool value);
    
    bool IsPartitionHead(const std::vector<uint8_t>& payload) const;
    
    enum class PacketType {
        SingleNALU,
        FragmentationUnit,
        AggregationPacket,
        PACIPacket
    };

    PacketType GetPacketType() const;
    H265SingleNALUnitPacket* GetSingleNALUPacket();
    H265FragmentationUnitPacket* GetFragmentationUnitPacket();
    H265AggregationPacket* GetAggregationPacket();
    H265PACIPacket* GetPACIPacket();

private:
    std::unique_ptr<H265PacketBase> packet_;
    PacketType packet_type_;
    bool might_need_donl_ = false;
};

// H.265 constants
constexpr uint8_t kH265NaluHeaderSize = 2;
constexpr uint8_t kH265NaluAggregationPacketType = 48;
constexpr uint8_t kH265NaluFragmentationUnitType = 49;
constexpr uint8_t kH265NaluPACIPacketType = 50;
constexpr uint8_t kH265FragmentationUnitHeaderSize = 1;

} // namespace rtp

#endif // H265_PACKET_H_