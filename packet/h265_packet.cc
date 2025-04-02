#include "h265_packet.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace rtp {

//
// H265NALUHeader implementation
//

H265NALUHeader::H265NALUHeader(uint16_t value) : value_(value) {}

H265NALUHeader::H265NALUHeader(uint8_t highByte, uint8_t lowByte) {
    value_ = (static_cast<uint16_t>(highByte) << 8) | static_cast<uint16_t>(lowByte);
}

bool H265NALUHeader::F() const {
    return (value_ >> 15) != 0;
}

uint8_t H265NALUHeader::Type() const {
    constexpr uint16_t mask = 0b01111110 << 8;
    return static_cast<uint8_t>((value_ & mask) >> (8 + 1));
}

bool H265NALUHeader::IsTypeVCLUnit() const {
    constexpr uint8_t msbMask = 0b00100000;
    return (Type() & msbMask) == 0;
}

uint8_t H265NALUHeader::LayerID() const {
    constexpr uint16_t mask = (0b00000001 << 8) | 0b11111000;
    return static_cast<uint8_t>((value_ & mask) >> 3);
}

uint8_t H265NALUHeader::TID() const {
    constexpr uint16_t mask = 0b00000111;
    return static_cast<uint8_t>(value_ & mask);
}

bool H265NALUHeader::IsAggregationPacket() const {
    return Type() == kH265NaluAggregationPacketType;
}

bool H265NALUHeader::IsFragmentationUnit() const {
    return Type() == kH265NaluFragmentationUnitType;
}

bool H265NALUHeader::IsPACIPacket() const {
    return Type() == kH265NaluPACIPacketType;
}

//
// H265FragmentationUnitHeader implementation
//

H265FragmentationUnitHeader::H265FragmentationUnitHeader(uint8_t value) : value_(value) {}

bool H265FragmentationUnitHeader::S() const {
    constexpr uint8_t mask = 0b10000000;
    return ((value_ & mask) >> 7) != 0;
}

bool H265FragmentationUnitHeader::E() const {
    constexpr uint8_t mask = 0b01000000;
    return ((value_ & mask) >> 6) != 0;
}

uint8_t H265FragmentationUnitHeader::FuType() const {
    constexpr uint8_t mask = 0b00111111;
    return value_ & mask;
}

//
// H265TSCI implementation
//

H265TSCI::H265TSCI(uint32_t value) : value_(value) {}

uint8_t H265TSCI::TL0PICIDX() const {
    constexpr uint32_t m1 = 0xFFFF0000;
    constexpr uint32_t m2 = 0xFF00;
    return static_cast<uint8_t>(((value_ & m1) >> 16) & m2) >> 8;
}

uint8_t H265TSCI::IrapPicID() const {
    constexpr uint32_t m1 = 0xFFFF0000;
    constexpr uint32_t m2 = 0x00FF;
    return static_cast<uint8_t>((value_ & m1) >> 16) & m2;
}

bool H265TSCI::S() const {
    constexpr uint32_t m1 = 0xFF00;
    constexpr uint32_t m2 = 0b10000000;
    return (static_cast<uint8_t>((value_ & m1) >> 8) & m2) != 0;
}

bool H265TSCI::E() const {
    constexpr uint32_t m1 = 0xFF00;
    constexpr uint32_t m2 = 0b01000000;
    return (static_cast<uint8_t>((value_ & m1) >> 8) & m2) != 0;
}

uint8_t H265TSCI::RES() const {
    constexpr uint32_t m1 = 0xFF00;
    constexpr uint32_t m2 = 0b00111111;
    return static_cast<uint8_t>((value_ & m1) >> 8) & m2;
}

//
// H265SingleNALUnitPacket implementation
//

H265SingleNALUnitPacket::H265SingleNALUnitPacket() = default;

bool H265SingleNALUnitPacket::Unmarshal(const std::vector<uint8_t>& payload) {
    constexpr size_t totalHeaderSize = kH265NaluHeaderSize;
    
    if (payload.empty()) {
        return false; // Nil packet
    }
    
    if (payload.size() <= totalHeaderSize) {
        return false; // Short packet
    }

    payload_header_ = H265NALUHeader(payload[0], payload[1]);
    
    if (payload_header_.F()) {
        return false; // Corrupted H265 packet
    }
    
    if (payload_header_.IsFragmentationUnit() || 
        payload_header_.IsPACIPacket() || 
        payload_header_.IsAggregationPacket()) {
        return false; // Invalid H265 packet type
    }

    auto data = payload;
    data.erase(data.begin(), data.begin() + 2);

    if (might_need_donl_) {
        if (data.size() <= 2) {
            return false; // Short packet
        }

        uint16_t donl_val = (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
        donl_ = std::make_unique<uint16_t>(donl_val);
        data.erase(data.begin(), data.begin() + 2);
    }

    payload_ = data;
    return true;
}

void H265SingleNALUnitPacket::WithDONL(bool value) {
    might_need_donl_ = value;
}

H265NALUHeader H265SingleNALUnitPacket::PayloadHeader() const {
    return payload_header_;
}

const uint16_t* H265SingleNALUnitPacket::DONL() const {
    return donl_.get();
}

const std::vector<uint8_t>& H265SingleNALUnitPacket::Payload() const {
    return payload_;
}

//
// H265AggregationUnitFirst implementation
//

H265AggregationUnitFirst::H265AggregationUnitFirst() = default;

const uint16_t* H265AggregationUnitFirst::DONL() const {
    return donl_.get();
}

uint16_t H265AggregationUnitFirst::NALUSize() const {
    return nalu_size_;
}

const std::vector<uint8_t>& H265AggregationUnitFirst::NalUnit() const {
    return nal_unit_;
}

//
// H265AggregationUnit implementation
//

H265AggregationUnit::H265AggregationUnit() = default;

const uint8_t* H265AggregationUnit::DOND() const {
    return dond_.get();
}

uint16_t H265AggregationUnit::NALUSize() const {
    return nalu_size_;
}

const std::vector<uint8_t>& H265AggregationUnit::NalUnit() const {
    return nal_unit_;
}

//
// H265AggregationPacket implementation
//

H265AggregationPacket::H265AggregationPacket() = default;

bool H265AggregationPacket::Unmarshal(const std::vector<uint8_t>& payload) {
    constexpr size_t totalHeaderSize = kH265NaluHeaderSize;
    
    if (payload.empty()) {
        return false; // Nil packet
    }
    
    if (payload.size() <= totalHeaderSize) {
        return false; // Short packet
    }

    H265NALUHeader payloadHeader(payload[0], payload[1]);
    
    if (payloadHeader.F()) {
        return false; // Corrupted H265 packet
    }
    
    if (!payloadHeader.IsAggregationPacket()) {
        return false; // Invalid H265 packet type
    }

    // Parse the first aggregation unit
    auto data = payload;
    data.erase(data.begin(), data.begin() + 2);
    
    first_unit_ = std::make_unique<H265AggregationUnitFirst>();

    if (might_need_donl_) {
        if (data.size() < 2) {
            return false; // Short packet
        }

        uint16_t donl_val = (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
        first_unit_->donl_ = std::make_unique<uint16_t>(donl_val);
        data.erase(data.begin(), data.begin() + 2);
    }
    
    if (data.size() < 2) {
        return false; // Short packet
    }
    
    first_unit_->nalu_size_ = (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
    data.erase(data.begin(), data.begin() + 2);

    if (data.size() < first_unit_->nalu_size_) {
        return false; // Short packet
    }

    first_unit_->nal_unit_.assign(data.begin(), data.begin() + first_unit_->nalu_size_);
    data.erase(data.begin(), data.begin() + first_unit_->nalu_size_);

    // Parse remaining Aggregation Units
    other_units_.clear();
    
    while (!data.empty()) {
        H265AggregationUnit unit;

        if (might_need_donl_) {
            if (data.size() < 1) {
                break;
            }

            unit.dond_ = std::make_unique<uint8_t>(data[0]);
            data.erase(data.begin());
        }

        if (data.size() < 2) {
            break;
        }
        
        unit.nalu_size_ = (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
        data.erase(data.begin(), data.begin() + 2);

        if (data.size() < unit.nalu_size_) {
            break;
        }

        unit.nal_unit_.assign(data.begin(), data.begin() + unit.nalu_size_);
        data.erase(data.begin(), data.begin() + unit.nalu_size_);

        other_units_.push_back(std::move(unit));
    }

    // There need to be at least one "other" Aggregation Unit
    if (other_units_.empty()) {
        return false; // Short packet
    }

    return true;
}

void H265AggregationPacket::WithDONL(bool value) {
    might_need_donl_ = value;
}

const H265AggregationUnitFirst* H265AggregationPacket::FirstUnit() const {
    return first_unit_.get();
}

const std::vector<H265AggregationUnit>& H265AggregationPacket::OtherUnits() const {
    return other_units_;
}

//
// H265FragmentationUnitPacket implementation
//

H265FragmentationUnitPacket::H265FragmentationUnitPacket() = default;

bool H265FragmentationUnitPacket::Unmarshal(const std::vector<uint8_t>& payload) {
    constexpr size_t totalHeaderSize = kH265NaluHeaderSize + kH265FragmentationUnitHeaderSize;
    
    if (payload.empty()) {
        return false; // Nil packet
    }
    
    if (payload.size() <= totalHeaderSize) {
        return false; // Short packet
    }

    payload_header_ = H265NALUHeader(payload[0], payload[1]);
    
    if (payload_header_.F()) {
        return false; // Corrupted H265 packet
    }
    
    if (!payload_header_.IsFragmentationUnit()) {
        return false; // Invalid H265 packet type
    }

    fu_header_ = H265FragmentationUnitHeader(payload[2]);
    
    auto data = payload;
    data.erase(data.begin(), data.begin() + 3);

    if (fu_header_.S() && might_need_donl_) {
        if (data.size() <= 2) {
            return false; // Short packet
        }

        uint16_t donl_val = (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
        donl_ = std::make_unique<uint16_t>(donl_val);
        data.erase(data.begin(), data.begin() + 2);
    }

    payload_ = data;
    return true;
}

void H265FragmentationUnitPacket::WithDONL(bool value) {
    might_need_donl_ = value;
}

H265NALUHeader H265FragmentationUnitPacket::PayloadHeader() const {
    return payload_header_;
}

H265FragmentationUnitHeader H265FragmentationUnitPacket::FuHeader() const {
    return fu_header_;
}

const uint16_t* H265FragmentationUnitPacket::DONL() const {
    return donl_.get();
}

const std::vector<uint8_t>& H265FragmentationUnitPacket::Payload() const {
    return payload_;
}

//
// H265PACIPacket implementation
//

H265PACIPacket::H265PACIPacket() = default;

bool H265PACIPacket::Unmarshal(const std::vector<uint8_t>& payload) {
    constexpr size_t totalHeaderSize = kH265NaluHeaderSize + 2;
    
    if (payload.empty()) {
        return false; // Nil packet
    }
    
    if (payload.size() <= totalHeaderSize) {
        return false; // Short packet
    }

    payload_header_ = H265NALUHeader(payload[0], payload[1]);
    
    if (payload_header_.F()) {
        return false; // Corrupted H265 packet
    }
    
    if (!payload_header_.IsPACIPacket()) {
        return false; // Invalid H265 packet type
    }

    paci_header_fields_ = (static_cast<uint16_t>(payload[2]) << 8) | static_cast<uint16_t>(payload[3]);
    
    auto data = payload;
    data.erase(data.begin(), data.begin() + 4);
    
    uint8_t header_extension_size = PHSsize();

    if (data.size() < header_extension_size + 1) {
        paci_header_fields_ = 0;
        return false; // Short packet
    }

    if (header_extension_size > 0) {
        phes_.assign(data.begin(), data.begin() + header_extension_size);
    }

    data.erase(data.begin(), data.begin() + header_extension_size);
    payload_ = data;
    
    return true;
}

H265NALUHeader H265PACIPacket::PayloadHeader() const {
    return payload_header_;
}

bool H265PACIPacket::A() const {
    constexpr uint16_t mask = 0b10000000 << 8;
    return (paci_header_fields_ & mask) != 0;
}

uint8_t H265PACIPacket::CType() const {
    constexpr uint16_t mask = 0b01111110 << 8;
    return static_cast<uint8_t>((paci_header_fields_ & mask) >> (8 + 1));
}

uint8_t H265PACIPacket::PHSsize() const {
    constexpr uint16_t mask = (0b00000001 << 8) | 0b11110000;
    return static_cast<uint8_t>((paci_header_fields_ & mask) >> 4);
}

bool H265PACIPacket::F0() const {
    constexpr uint16_t mask = 0b00001000;
    return (paci_header_fields_ & mask) != 0;
}

bool H265PACIPacket::F1() const {
    constexpr uint16_t mask = 0b00000100;
    return (paci_header_fields_ & mask) != 0;
}

bool H265PACIPacket::F2() const {
    constexpr uint16_t mask = 0b00000010;
    return (paci_header_fields_ & mask) != 0;
}

bool H265PACIPacket::Y() const {
    constexpr uint16_t mask = 0b00000001;
    return (paci_header_fields_ & mask) != 0;
}

const std::vector<uint8_t>& H265PACIPacket::PHES() const {
    return phes_;
}

const std::vector<uint8_t>& H265PACIPacket::Payload() const {
    return payload_;
}

const H265TSCI* H265PACIPacket::TSCI() const {
    if (!F0() || PHSsize() < 3) {
        return nullptr;
    }

    if (!tsci_ && phes_.size() >= 3) {
        uint32_t value = (static_cast<uint32_t>(phes_[0]) << 16) | 
                         (static_cast<uint32_t>(phes_[1]) << 8) | 
                          static_cast<uint32_t>(phes_[2]);
        tsci_ = std::make_unique<H265TSCI>(value);
    }

    return tsci_.get();
}

//
// H265Packet implementation
//

H265Packet::H265Packet() : packet_type_(PacketType::SingleNALU) {}

bool H265Packet::Unmarshal(const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
        return false; // Nil packet
    }
    
    if (payload.size() <= kH265NaluHeaderSize) {
        return false; // Short packet
    }

    H265NALUHeader payloadHeader(payload[0], payload[1]);
    if (payloadHeader.F()) {
        return false; // Corrupted H265 packet
    }

    bool result = false;

    switch (payloadHeader.Type()) {
        case kH265NaluPACIPacketType:
            packet_ = std::make_unique<H265PACIPacket>();
            packet_type_ = PacketType::PACIPacket;
            result = packet_->Unmarshal(payload);
            break;

        case kH265NaluFragmentationUnitType:
            {
                auto fu_packet = std::make_unique<H265FragmentationUnitPacket>();
                fu_packet->WithDONL(might_need_donl_);
                packet_ = std::move(fu_packet);
                packet_type_ = PacketType::FragmentationUnit;
                result = packet_->Unmarshal(payload);
            }
            break;

        case kH265NaluAggregationPacketType:
            {
                auto agg_packet = std::make_unique<H265AggregationPacket>();
                agg_packet->WithDONL(might_need_donl_);
                packet_ = std::move(agg_packet);
                packet_type_ = PacketType::AggregationPacket;
                result = packet_->Unmarshal(payload);
            }
            break;

        default:
            {
                auto single_packet = std::make_unique<H265SingleNALUnitPacket>();
                single_packet->WithDONL(might_need_donl_);
                packet_ = std::move(single_packet);
                packet_type_ = PacketType::SingleNALU;
                result = packet_->Unmarshal(payload);
            }
            break;
    }

    return result;
}

void H265Packet::WithDONL(bool value) {
    might_need_donl_ = value;
}

bool H265Packet::IsPartitionHead(const std::vector<uint8_t>& payload) const {
    if (payload.size() < 3) {
        return false;
    }

    H265NALUHeader header(payload[0], payload[1]);
    if (header.Type() == kH265NaluFragmentationUnitType) {
        return H265FragmentationUnitHeader(payload[2]).S();
    }

    return true;
}

H265Packet::PacketType H265Packet::GetPacketType() const {
    return packet_type_;
}

H265SingleNALUnitPacket* H265Packet::GetSingleNALUPacket() {
    if (packet_type_ != PacketType::SingleNALU) {
        return nullptr;
    }
    return static_cast<H265SingleNALUnitPacket*>(packet_.get());
}

H265FragmentationUnitPacket* H265Packet::GetFragmentationUnitPacket() {
    if (packet_type_ != PacketType::FragmentationUnit) {
        return nullptr;
    }
    return static_cast<H265FragmentationUnitPacket*>(packet_.get());
}

H265AggregationPacket* H265Packet::GetAggregationPacket() {
    if (packet_type_ != PacketType::AggregationPacket) {
        return nullptr;
    }
    return static_cast<H265AggregationPacket*>(packet_.get());
}

H265PACIPacket* H265Packet::GetPACIPacket() {
    if (packet_type_ != PacketType::PACIPacket) {
        return nullptr;
    }
    return static_cast<H265PACIPacket*>(packet_.get());
}

} // namespace rtp