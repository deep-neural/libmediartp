#include "h265_packetizer.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace rtp {

//
// H265Payloader implementation
//

H265Payloader::H265Payloader() = default;

void H265Payloader::EmitNALU(const std::vector<uint8_t>& nalu, 
                           const std::function<void(const std::vector<uint8_t>&)>& emit_func) {
    if (nalu.size() >= kH265NaluHeaderSize) {
        emit_func(nalu);
    }
}

void H265Payloader::WithDONL(bool value) {
    add_donl_ = value;
}

void H265Payloader::WithSkipAggregation(bool value) {
    skip_aggregation_ = value;
}

std::vector<std::vector<uint8_t>> H265Payloader::Payload(uint16_t mtu, const std::vector<uint8_t>& payload) {
    std::vector<std::vector<uint8_t>> payloads;
    if (payload.empty() || mtu == 0) {
        return payloads;
    }

    std::vector<std::vector<uint8_t>> bufferedNALUs;
    int aggregationBufferSize = 0;

    auto flushBufferedNals = [&]() {
        if (bufferedNALUs.empty()) {
            return;
        }
        
        if (bufferedNALUs.size() == 1) {
            // Emit this as a single NALU packet
            std::vector<uint8_t> nalu = bufferedNALUs[0];

            if (add_donl_) {
                std::vector<uint8_t> buf(nalu.size() + 2);

                // Copy the NALU header to the payload header
                std::copy(nalu.begin(), nalu.begin() + kH265NaluHeaderSize, buf.begin());

                // Copy the DONL into the header
                buf[kH265NaluHeaderSize] = static_cast<uint8_t>(donl_ >> 8);
                buf[kH265NaluHeaderSize + 1] = static_cast<uint8_t>(donl_ & 0xFF);

                // Write the payload
                std::copy(nalu.begin() + kH265NaluHeaderSize, nalu.end(), buf.begin() + kH265NaluHeaderSize + 2);

                donl_++;
                payloads.push_back(buf);
            } else {
                // Write the nalu directly to the payload
                payloads.push_back(nalu);
            }
        } else {
            // Construct an aggregation packet
            std::vector<uint8_t> buf(aggregationBufferSize);

            uint8_t layerID = 255;
            uint8_t tid = 255;
            
            for (const auto& nalu : bufferedNALUs) {
                H265NALUHeader header(nalu[0], nalu[1]);
                uint8_t headerLayerID = header.LayerID();
                uint8_t headerTID = header.TID();
                
                if (headerLayerID < layerID) {
                    layerID = headerLayerID;
                }
                
                if (headerTID < tid) {
                    tid = headerTID;
                }
            }

            // Create the header for the aggregation packet
            uint16_t header_value = (static_cast<uint16_t>(kH265NaluAggregationPacketType) << 9) |
                                   (static_cast<uint16_t>(layerID) << 3) |
                                   static_cast<uint16_t>(tid);
                                   
            buf[0] = static_cast<uint8_t>(header_value >> 8);
            buf[1] = static_cast<uint8_t>(header_value & 0xFF);

            size_t index = 2;
            for (size_t i = 0; i < bufferedNALUs.size(); ++i) {
                const auto& nalu = bufferedNALUs[i];
                
                if (add_donl_) {
                    if (i == 0) {
                        buf[index++] = static_cast<uint8_t>(donl_ >> 8);
                        buf[index++] = static_cast<uint8_t>(donl_ & 0xFF);
                    } else {
                        buf[index++] = static_cast<uint8_t>(i - 1);
                    }
                }

                // Write NALU size
                uint16_t nalu_size = static_cast<uint16_t>(nalu.size());
                buf[index++] = static_cast<uint8_t>(nalu_size >> 8);
                buf[index++] = static_cast<uint8_t>(nalu_size & 0xFF);
                
                // Copy NALU data
                std::copy(nalu.begin(), nalu.end(), buf.begin() + index);
                index += nalu.size();
            }
            
            payloads.push_back(buf);
        }
        
        // Clear buffered NALUs
        bufferedNALUs.clear();
        aggregationBufferSize = 0;
    };

    auto calcMarginalAggregationSize = [&](const std::vector<uint8_t>& nalu) -> int {
        int marginalAggregationSize = static_cast<int>(nalu.size()) + 2; // +2 is NALU size Field size
        
        if (bufferedNALUs.size() == 1) {
            marginalAggregationSize = static_cast<int>(nalu.size()) + 4; // +4 are Aggregation header + NALU size Field size
        }
        
        if (add_donl_) {
            if (bufferedNALUs.empty()) {
                marginalAggregationSize += 2;
            } else {
                marginalAggregationSize += 1;
            }
        }

        return marginalAggregationSize;
    };

    // Scan through payload to find NAL units
    size_t offset = 0;
    while (offset < payload.size()) {
        // Find start code
        size_t naluStart = offset;
        size_t naluSize = 0;
        
        // Find the next start code or end of data
        size_t nextStart = payload.size();
        for (size_t i = offset + 3; i < payload.size() - 2; ++i) {
            if (payload[i] == 0 && payload[i + 1] == 0 && 
                (payload[i + 2] == 1 || (payload[i + 2] == 0 && payload[i + 3] == 1))) {
                nextStart = i;
                break;
            }
        }
        
        // Calculate NALU size and skip start code
        if (payload[naluStart] == 0 && payload[naluStart + 1] == 0) {
            if (payload[naluStart + 2] == 1) {
                // 3-byte start code
                naluStart += 3;
            } else if (naluStart + 3 < payload.size() && payload[naluStart + 2] == 0 && payload[naluStart + 3] == 1) {
                // 4-byte start code
                naluStart += 4;
            }
        }
        
        naluSize = nextStart - naluStart;
        
        if (naluSize > 0) {
            std::vector<uint8_t> nalu(payload.begin() + naluStart, payload.begin() + naluStart + naluSize);
            
            if (nalu.size() >= kH265NaluHeaderSize) {
                int naluLen = static_cast<int>(nalu.size()) + kH265NaluHeaderSize;
                if (add_donl_) {
                    naluLen += 2;
                }
                
                if (naluLen <= mtu) {
                    // This NALU fits into a single packet, either it can be emitted as
                    // a single NALU or appended to the previous aggregation packet
                    int marginalAggregationSize = calcMarginalAggregationSize(nalu);

                    if (aggregationBufferSize + marginalAggregationSize > mtu) {
                        flushBufferedNals();
                        marginalAggregationSize = calcMarginalAggregationSize(nalu);
                    }
                    
                    bufferedNALUs.push_back(nalu);
                    aggregationBufferSize += marginalAggregationSize;
                    
                    if (skip_aggregation_) {
                        // Emit this immediately
                        flushBufferedNals();
                    }
                } else {
                    // If this NALU doesn't fit in the current MTU, it needs to be fragmented
                    int fuPacketHeaderSize = kH265FragmentationUnitHeaderSize + kH265NaluHeaderSize;
                    if (add_donl_) {
                        fuPacketHeaderSize += 2;
                    }

                    // Then, fragment the NALU
                    int maxFUPayloadSize = mtu - fuPacketHeaderSize;

                    H265NALUHeader naluHeader(nalu[0], nalu[1]);

                    // The NALU header is omitted from the fragmentation packet payload
                    std::vector<uint8_t> naluData(nalu.begin() + kH265NaluHeaderSize, nalu.end());

                    if (maxFUPayloadSize <= 0 || naluData.empty()) {
                        continue;
                    }

                    // Flush any buffered aggregation packets
                    flushBufferedNals();

                    size_t fullNALUSize = naluData.size();
                    size_t offset = 0;
                    
                    while (offset < naluData.size()) {
                        size_t currentFUPayloadSize = naluData.size() - offset;
                        if (currentFUPayloadSize > static_cast<size_t>(maxFUPayloadSize)) {
                            currentFUPayloadSize = maxFUPayloadSize;
                        }

                        std::vector<uint8_t> out(fuPacketHeaderSize + currentFUPayloadSize);

                        // Write the payload header
                        uint16_t header_value = naluHeader.GetValue();
                        out[0] = (static_cast<uint8_t>(header_value >> 8) & 0b10000001) | 
                                 (kH265NaluFragmentationUnitType << 1);
                        out[1] = static_cast<uint8_t>(header_value & 0xFF);

                        // Write the fragment header
                        out[2] = naluHeader.Type();
                        if (offset == 0) {
                            // Set start bit
                            out[2] |= 1 << 7;
                        } else if (offset + currentFUPayloadSize == fullNALUSize) {
                            // Set end bit
                            out[2] |= 1 << 6;
                        }

                        if (add_donl_) {
                            // Write the DONL header
                            out[3] = static_cast<uint8_t>(donl_ >> 8);
                            out[4] = static_cast<uint8_t>(donl_ & 0xFF);
                            donl_++;

                            // Copy the fragment payload
                            std::copy(naluData.begin() + offset, 
                                     naluData.begin() + offset + currentFUPayloadSize,
                                     out.begin() + 5);
                        } else {
                            // Copy the fragment payload
                            std::copy(naluData.begin() + offset,
                                     naluData.begin() + offset + currentFUPayloadSize,
                                     out.begin() + 3);
                        }

                        // Append the fragment to the payload
                        payloads.push_back(out);

                        // Advance the NALU data pointer
                        offset += currentFUPayloadSize;
                    }
                }
            }
        }
        
        offset = nextStart;
    }

    flushBufferedNals();
    return payloads;
}

//
// H265Packetizer implementation
//

H265Packetizer::H265Packetizer(uint16_t mtu) 
    : mtu_(mtu),
      sequencer_(std::make_shared<RandomSequencer>()) {
}

void H265Packetizer::WithDONL(bool value) {
    payloader_.WithDONL(value);
}

void H265Packetizer::WithSkipAggregation(bool value) {
    payloader_.WithSkipAggregation(value);
}

void H265Packetizer::WithSequencer(std::shared_ptr<Sequencer> sequencer) {
    if (sequencer) {
        sequencer_ = sequencer;
    }
}

void H265Packetizer::WithTimestamp(uint32_t timestamp) {
    timestamp_ = timestamp;
}

void H265Packetizer::WithSSRC(uint32_t ssrc) {
    ssrc_ = ssrc;
}

void H265Packetizer::WithPayloadType(uint8_t payload_type) {
    payload_type_ = payload_type;
}

bool H265Packetizer::Packetize(const std::vector<uint8_t>& h265_frame, std::vector<std::vector<uint8_t>>* rtp_packets) {
    if (!rtp_packets) {
        return false;
    }
    
    rtp_packets->clear();
    
    // Let the payloader process the frames into RTP sized chunks
    std::vector<std::vector<uint8_t>> payloads = payloader_.Payload(mtu_, h265_frame);
    
    if (payloads.empty()) {
        return false;
    }
    
    // For each payload, create an RTP packet
    for (size_t i = 0; i < payloads.size(); ++i) {
        const auto& payload = payloads[i];
        
        Packet packet;
        packet.header.payload_type = payload_type_;
        packet.header.sequence_number = sequencer_->NextSequenceNumber();
        packet.header.timestamp = timestamp_;
        packet.header.ssrc = ssrc_;
        
        // Set marker bit on the last packet
        packet.header.marker = (i == payloads.size() - 1);
        
        packet.payload = payload;
        
        // Serialize the packet
        std::vector<uint8_t> rtp_packet = packet.Packetize();
        rtp_packets->push_back(rtp_packet);
    }
    
    return true;
}

} // namespace rtp