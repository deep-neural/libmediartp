#include "rtp_packet.h"
#include <cstring>
#include <stdexcept>
#include <random>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <mutex>

namespace rtp {

// Helper functions for big-endian operations
namespace {
    uint16_t NetworkToHost16(const uint8_t* data) {
        return static_cast<uint16_t>((data[0] << 8) | data[1]);
    }

    uint32_t NetworkToHost32(const uint8_t* data) {
        return (static_cast<uint32_t>(data[0]) << 24) |
               (static_cast<uint32_t>(data[1]) << 16) |
               (static_cast<uint32_t>(data[2]) << 8) |
               static_cast<uint32_t>(data[3]);
    }

    void HostToNetwork16(uint16_t value, uint8_t* data) {
        data[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
        data[1] = static_cast<uint8_t>(value & 0xFF);
    }

    void HostToNetwork32(uint32_t value, uint8_t* data) {
        data[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
        data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
        data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
        data[3] = static_cast<uint8_t>(value & 0xFF);
    }

    // Global random generator with seed
    std::mt19937 CreateRandomGenerator() {
        auto seed = std::chrono::system_clock::now().time_since_epoch().count();
        return std::mt19937(static_cast<unsigned int>(seed));
    }
    
    std::mt19937 global_random_generator = CreateRandomGenerator();
    std::mutex random_mutex;
}

// Header implementation
Header::Header() : version(2) {}

bool Header::Depacketize(const std::vector<uint8_t>& buf, size_t* bytes_read) {
    if (buf.size() < kHeaderLength) {
        return false;
    }

    version = (buf[0] >> kVersionShift) & kVersionMask;
    padding = ((buf[0] >> kPaddingShift) & kPaddingMask) > 0;
    extension = ((buf[0] >> kExtensionShift) & kExtensionMask) > 0;
    size_t csrc_count = buf[0] & kCCMask;
    
    marker = ((buf[1] >> kMarkerShift) & kMarkerMask) > 0;
    payload_type = buf[1] & kPayloadTypeMask;
    
    size_t n = kCsrcOffset + (csrc_count * kCsrcLength);
    if (buf.size() < n) {
        return false;
    }
    
    sequence_number = NetworkToHost16(&buf[kSeqNumOffset]);
    timestamp = NetworkToHost32(&buf[kTimestampOffset]);
    ssrc = NetworkToHost32(&buf[kSsrcOffset]);
    
    // Extract CSRC list
    csrc.resize(csrc_count);
    for (size_t i = 0; i < csrc_count; i++) {
        size_t offset = kCsrcOffset + (i * kCsrcLength);
        csrc[i] = NetworkToHost32(&buf[offset]);
    }
    
    extensions.clear();
    
    if (extension) {
        if (buf.size() < n + 4) {
            return false;
        }
        
        extension_profile = NetworkToHost16(&buf[n]);
        n += 2;
        size_t extension_length = NetworkToHost16(&buf[n]) * 4;
        n += 2;
        size_t extension_end = n + extension_length;
        
        if (buf.size() < extension_end) {
            return false;
        }
        
        if (extension_profile == kExtensionProfileOneByte || 
            extension_profile == kExtensionProfileTwoByte) {
            uint8_t ext_id;
            size_t payload_len;
            
            while (n < extension_end) {
                if (buf[n] == 0x00) { // padding
                    n++;
                    continue;
                }
                
                if (extension_profile == kExtensionProfileOneByte) {
                    ext_id = buf[n] >> 4;
                    payload_len = static_cast<size_t>((buf[n] & 0x0F) + 1);
                    n++;
                    
                    if (ext_id == kExtensionIDReserved) {
                        break;
                    }
                } else { // Two-byte profile
                    ext_id = buf[n];
                    n++;
                    
                    if (n >= buf.size()) {
                        return false;
                    }
                    
                    payload_len = buf[n];
                    n++;
                }
                
                if (n + payload_len > buf.size()) {
                    return false;
                }
                
                std::vector<uint8_t> ext_payload(buf.begin() + n, buf.begin() + n + payload_len);
                extensions.emplace_back(ext_id, ext_payload);
                n += payload_len;
            }
        } else {
            // RFC3550 Extension
            std::vector<uint8_t> ext_payload(buf.begin() + n, buf.begin() + extension_end);
            extensions.emplace_back(0, ext_payload);
            n = extension_end;
        }
    }
    
    if (bytes_read) {
        *bytes_read = n;
    }
    
    return true;
}

std::vector<uint8_t> Header::Packetize() const {
    std::vector<uint8_t> buf(PacketSize());
    PacketizeTo(&buf);
    return buf;
}

bool Header::PacketizeTo(std::vector<uint8_t>* buf) const {
    if (!buf) {
        return false;
    }
    
    size_t size = PacketSize();
    if (buf->size() < size) {
        buf->resize(size);
    }
    
    uint8_t& first_byte = (*buf)[0];
    first_byte = (version << kVersionShift) | (csrc.size() & kCCMask);
    if (padding) {
        first_byte |= (1 << kPaddingShift);
    }
    if (extension) {
        first_byte |= (1 << kExtensionShift);
    }
    
    (*buf)[1] = payload_type;
    if (marker) {
        (*buf)[1] |= (1 << kMarkerShift);
    }
    
    HostToNetwork16(sequence_number, &(*buf)[kSeqNumOffset]);
    HostToNetwork32(timestamp, &(*buf)[kTimestampOffset]);
    HostToNetwork32(ssrc, &(*buf)[kSsrcOffset]);
    
    size_t n = kCsrcOffset;
    for (uint32_t csrc_val : csrc) {
        HostToNetwork32(csrc_val, &(*buf)[n]);
        n += kCsrcLength;
    }
    
    if (extension) {
        size_t ext_header_pos = n;
        HostToNetwork16(extension_profile, &(*buf)[n]);
        n += 2;
        size_t ext_length_pos = n;
        n += 2; // Skip length for now
        size_t start_extensions_pos = n;
        
        switch (extension_profile) {
            case kExtensionProfileOneByte:
                for (const auto& ext : extensions) {
                    if (ext.id() < 1 || ext.id() > 14) {
                        return false;
                    }
                    if (ext.payload().size() > 16) {
                        return false;
                    }
                    
                    (*buf)[n++] = (ext.id() << 4) | ((ext.payload().size() - 1) & 0x0F);
                    std::copy(ext.payload().begin(), ext.payload().end(), buf->begin() + n);
                    n += ext.payload().size();
                }
                break;
                
            case kExtensionProfileTwoByte:
                for (const auto& ext : extensions) {
                    if (ext.id() < 1) {
                        return false;
                    }
                    if (ext.payload().size() > 255) {
                        return false;
                    }
                    
                    (*buf)[n++] = ext.id();
                    (*buf)[n++] = static_cast<uint8_t>(ext.payload().size());
                    std::copy(ext.payload().begin(), ext.payload().end(), buf->begin() + n);
                    n += ext.payload().size();
                }
                break;
                
            default: // RFC3550
                if (!extensions.empty()) {
                    const auto& payload = extensions[0].payload();
                    if (payload.size() % 4 != 0) {
                        return false;
                    }
                    std::copy(payload.begin(), payload.end(), buf->begin() + n);
                    n += payload.size();
                }
                break;
        }
        
        // Calculate extension size and round to 4 bytes
        size_t ext_size = n - start_extensions_pos;
        size_t rounded_ext_size = ((ext_size + 3) / 4) * 4;
        
        // Set the extension length (in 32-bit words)
        HostToNetwork16(static_cast<uint16_t>(rounded_ext_size / 4), &(*buf)[ext_length_pos]);
        
        // Add padding to reach 4 byte boundary
        for (size_t i = 0; i < rounded_ext_size - ext_size; i++) {
            (*buf)[n++] = 0;
        }
    }
    
    return true;
}

size_t Header::PacketSize() const {
    size_t size = kCsrcOffset + (csrc.size() * kCsrcLength);
    
    if (extension) {
        size_t ext_size = 4; // 4 bytes for profile and length
        
        switch (extension_profile) {
            case kExtensionProfileOneByte:
                for (const auto& ext : extensions) {
                    ext_size += 1 + ext.payload().size();
                }
                break;
                
            case kExtensionProfileTwoByte:
                for (const auto& ext : extensions) {
                    ext_size += 2 + ext.payload().size();
                }
                break;
                
            default: // RFC3550
                if (!extensions.empty()) {
                    ext_size += extensions[0].payload().size();
                }
                break;
        }
        
        // Round up to multiple of 4 bytes
        size += ((ext_size + 3) / 4) * 4;
    }
    
    return size;
}

bool Header::SetExtension(uint8_t id, const std::vector<uint8_t>& payload) {
    if (extension) {
        switch (extension_profile) {
            case kExtensionProfileOneByte:
                if (id < 1 || id > 14) {
                    return false;
                }
                if (payload.size() > 16) {
                    return false;
                }
                break;
                
            case kExtensionProfileTwoByte:
                if (id < 1) {
                    return false;
                }
                if (payload.size() > 255) {
                    return false;
                }
                break;
                
            default: // RFC3550
                if (id != 0) {
                    return false;
                }
                break;
        }
        
        // Update existing extension if it exists
        for (size_t i = 0; i < extensions.size(); i++) {
            if (extensions[i].id() == id) {
                extensions[i] = Extension(id, payload);
                return true;
            }
        }
        
        // Add new extension
        extensions.emplace_back(id, payload);
        return true;
    }
    
    // No existing extensions, create first one
    extension = true;
    
    size_t payload_len = payload.size();
    if (payload_len <= 16) {
        extension_profile = kExtensionProfileOneByte;
    } else if (payload_len < 256) {
        extension_profile = kExtensionProfileTwoByte;
    }
    
    extensions.emplace_back(id, payload);
    return true;
}

std::vector<uint8_t> Header::GetExtension(uint8_t id) const {
    if (!extension) {
        return {};
    }
    
    for (const auto& ext : extensions) {
        if (ext.id() == id) {
            return ext.payload();
        }
    }
    
    return {};
}

std::vector<uint8_t> Header::GetExtensionIDs() const {
    if (!extension || extensions.empty()) {
        return {};
    }
    
    std::vector<uint8_t> ids;
    ids.reserve(extensions.size());
    for (const auto& ext : extensions) {
        ids.push_back(ext.id());
    }
    
    return ids;
}

bool Header::DeleteExtension(uint8_t id) {
    if (!extension) {
        return false;
    }
    
    for (auto it = extensions.begin(); it != extensions.end(); ++it) {
        if (it->id() == id) {
            extensions.erase(it);
            return true;
        }
    }
    
    return false;
}

Header Header::Clone() const {
    Header clone = *this;
    
    // Deep copy for vector fields
    clone.csrc = csrc;
    
    clone.extensions.clear();
    for (const auto& ext : extensions) {
        clone.extensions.emplace_back(ext.id(), ext.payload());
    }
    
    return clone;
}

// Packet implementation
Packet::Packet() {}

bool Packet::Depacketize(const std::vector<uint8_t>& buf) {
    size_t header_size;
    if (!header.Depacketize(buf, &header_size)) {
        return false;
    }
    
    size_t end = buf.size();
    if (header.padding) {
        if (end <= header_size) {
            return false;
        }
        padding_size = buf[end - 1];
        end -= padding_size;
    } else {
        padding_size = 0;
    }
    
    if (end < header_size) {
        return false;
    }
    
    payload.assign(buf.begin() + header_size, buf.begin() + end);
    return true;
}

std::vector<uint8_t> Packet::Packetize() const {
    std::vector<uint8_t> buf(PacketSize());
    PacketizeTo(&buf);
    return buf;
}

bool Packet::PacketizeTo(std::vector<uint8_t>* buf) const {
    if (!buf) {
        return false;
    }
    
    if (header.padding && padding_size == 0) {
        return false;
    }
    
    size_t size = PacketSize();
    if (buf->size() < size) {
        buf->resize(size);
    }
    
    if (!header.PacketizeTo(buf)) {
        return false;
    }
    
    size_t header_size = header.PacketSize();
    std::copy(payload.begin(), payload.end(), buf->begin() + header_size);
    
    if (header.padding) {
        (*buf)[header_size + payload.size() + padding_size - 1] = padding_size;
    }
    
    return true;
}

size_t Packet::PacketSize() const {
    return header.PacketSize() + payload.size() + padding_size;
}

std::shared_ptr<Packet> Packet::Clone() const {
    auto clone = std::make_shared<Packet>();
    clone->header = header.Clone();
    clone->payload = payload;
    clone->padding_size = padding_size;
    return clone;
}

std::string Packet::ToString() const {
    std::stringstream ss;
    ss << "RTP PACKET:\n";
    ss << "\tVersion: " << static_cast<int>(header.version) << "\n";
    ss << "\tMarker: " << (header.marker ? "true" : "false") << "\n";
    ss << "\tPayload Type: " << static_cast<int>(header.payload_type) << "\n";
    ss << "\tSequence Number: " << header.sequence_number << "\n";
    ss << "\tTimestamp: " << header.timestamp << "\n";
    ss << "\tSSRC: " << header.ssrc << " (0x" << std::hex << header.ssrc << std::dec << ")\n";
    ss << "\tPayload Length: " << payload.size() << "\n";
    return ss.str();
}

// RandomSequencer implementation
RandomSequencer::RandomSequencer() {
    std::lock_guard<std::mutex> lock(random_mutex);
    // Use only half the potential sequence number space to avoid issues with SRTP
    constexpr uint16_t max_initial_random_seq = (1 << 15) - 1;
    std::uniform_int_distribution<uint16_t> dist(0, max_initial_random_seq);
    sequence_number_ = dist(global_random_generator);
}

uint16_t RandomSequencer::NextSequenceNumber() {
    std::lock_guard<std::mutex> lock(mutex_);
    sequence_number_++;
    if (sequence_number_ == 0) {
        roll_over_count_++;
    }
    return sequence_number_;
}

uint64_t RandomSequencer::RollOverCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return roll_over_count_;
}

// FixedSequencer implementation
FixedSequencer::FixedSequencer(uint16_t starting_seq) 
    : sequence_number_(starting_seq - 1) {} // -1 because first call increments

uint16_t FixedSequencer::NextSequenceNumber() {
    std::lock_guard<std::mutex> lock(mutex_);
    sequence_number_++;
    if (sequence_number_ == 0) {
        roll_over_count_++;
    }
    return sequence_number_;
}

uint64_t FixedSequencer::RollOverCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return roll_over_count_;
}

} // namespace rtp