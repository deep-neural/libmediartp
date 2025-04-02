#include "h264_packetizer.h"
#include <algorithm>
#include <arpa/inet.h>

namespace rtp {

H264Packetizer::H264Packetizer(uint16_t mtu) : H264Packet(), mtu_(mtu) {}

H264Packetizer::H264Packetizer(uint16_t mtu, bool is_avc) : H264Packet(), mtu_(mtu) {
    is_avc_ = is_avc;
}

bool H264Packetizer::Packetize(const std::vector<uint8_t>& frame, std::vector<std::vector<uint8_t>>* packets) {
    if (!packets) {
        return false;
    }
    
    packets->clear();
    
    if (frame.empty()) {
        return true;
    }
    
    // Find and process NALUs in the frame
    std::vector<std::vector<uint8_t>> payloads;
    
    H264Packet::EmitNalus(frame, [this, &payloads](const std::vector<uint8_t>& nalu) {
        if (nalu.empty()) {
            return;
        }
        
        std::vector<std::vector<uint8_t>> nalu_payloads = this->Payload(nalu);
        payloads.insert(payloads.end(), nalu_payloads.begin(), nalu_payloads.end());
    });
    
    // Create RTP packets for each payload
    for (size_t i = 0; i < payloads.size(); i++) {
        Packet rtp_packet;
        
        // Set marker bit for the last packet
        rtp_packet.header.marker = (i == payloads.size() - 1);
        rtp_packet.payload = payloads[i];
        
        std::vector<uint8_t> serialized = rtp_packet.Packetize();
        packets->push_back(serialized);
    }
    
    return true;
}

std::vector<std::vector<uint8_t>> H264Packetizer::Payload(const std::vector<uint8_t>& nalu) {
    std::vector<std::vector<uint8_t>> payloads;
    
    if (nalu.empty()) {
        return payloads;
    }
    
    uint8_t nalu_type = nalu[0] & kNaluTypeBitmask;
    uint8_t nalu_ref_idc = nalu[0] & kNaluRefIdcBitmask;
    
    // Filter out specific NALU types
    if (nalu_type == kAudNALUType || nalu_type == kFillerNALUType) {
        return payloads;
    }
    
    // Handle SPS and PPS NALUs for potential STAP-A
    if (!disable_stap_a_) {
        if (nalu_type == kSpsNALUType) {
            sps_nalu_ = nalu;
            return payloads;
        } else if (nalu_type == kPpsNALUType) {
            pps_nalu_ = nalu;
            return payloads;
        }
        
        // If we have SPS and PPS NALUs, consider combining with the current NALU
        if (!sps_nalu_.empty() && !pps_nalu_.empty()) {
            // Pack current NALU with SPS and PPS as STAP-A
            uint16_t sps_len = htons(static_cast<uint16_t>(sps_nalu_.size()));
            uint16_t pps_len = htons(static_cast<uint16_t>(pps_nalu_.size()));
            uint16_t nalu_len = htons(static_cast<uint16_t>(nalu.size()));
            
            std::vector<uint8_t> stap_a_nalu = {kOutputStapAHeader};
            
            // Add SPS
            stap_a_nalu.push_back(static_cast<uint8_t>(sps_len >> 8));
            stap_a_nalu.push_back(static_cast<uint8_t>(sps_len & 0xFF));
            stap_a_nalu.insert(stap_a_nalu.end(), sps_nalu_.begin(), sps_nalu_.end());
            
            // Add PPS
            stap_a_nalu.push_back(static_cast<uint8_t>(pps_len >> 8));
            stap_a_nalu.push_back(static_cast<uint8_t>(pps_len & 0xFF));
            stap_a_nalu.insert(stap_a_nalu.end(), pps_nalu_.begin(), pps_nalu_.end());
            
            // Add current NALU
            stap_a_nalu.push_back(static_cast<uint8_t>(nalu_len >> 8));
            stap_a_nalu.push_back(static_cast<uint8_t>(nalu_len & 0xFF));
            stap_a_nalu.insert(stap_a_nalu.end(), nalu.begin(), nalu.end());
            
            if (stap_a_nalu.size() <= mtu_) {
                payloads.push_back(stap_a_nalu);
                
                // Clear stored NALUs
                sps_nalu_.clear();
                pps_nalu_.clear();
                
                return payloads;
            }
            
            // If STAP-A packet would be too large, clear stored NALUs
            sps_nalu_.clear();
            pps_nalu_.clear();
        }
    }
    
    // Single NALU
    if (nalu.size() <= mtu_) {
        std::vector<uint8_t> out = nalu;
        payloads.push_back(out);
        return payloads;
    }
    
    // FU-A fragmentation for large NALUs
    size_t max_fragment_size = mtu_ - kFuaHeaderSize;
    size_t nalu_index = 1; // Skip the first byte which contains the NALU header
    size_t nalu_length = nalu.size() - nalu_index;
    size_t nalu_remaining = nalu_length;
    
    while (nalu_remaining > 0) {
        size_t current_fragment_size = std::min(max_fragment_size, nalu_remaining);
        std::vector<uint8_t> out(kFuaHeaderSize + current_fragment_size);
        
        // Set FU indicator
        out[0] = kFuaNALUType | nalu_ref_idc;
        
        // Set FU header
        out[1] = nalu_type;
        if (nalu_remaining == nalu_length) {
            // Set start bit for first fragment
            out[1] |= kFuStartBitmask;
        } else if (nalu_remaining <= current_fragment_size) {
            // Set end bit for last fragment
            out[1] |= kFuEndBitmask;
        }
        
        // Copy fragment data
        std::copy(nalu.begin() + nalu_index, 
                  nalu.begin() + nalu_index + current_fragment_size, 
                  out.begin() + kFuaHeaderSize);
        
        payloads.push_back(out);
        
        nalu_remaining -= current_fragment_size;
        nalu_index += current_fragment_size;
    }
    
    return payloads;
}

} // namespace rtp