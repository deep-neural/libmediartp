#ifndef H265_PACKETIZER_H_
#define H265_PACKETIZER_H_

#include "rtp_packet.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include "h265_packet.h"

namespace rtp {

class H265Payloader {
public:
    H265Payloader();
    ~H265Payloader() = default;
    
    std::vector<std::vector<uint8_t>> Payload(uint16_t mtu, const std::vector<uint8_t>& payload);
    
    // Configure options
    void WithDONL(bool value);
    void WithSkipAggregation(bool value);
    
private:
    bool add_donl_ = false;
    bool skip_aggregation_ = false;
    uint16_t donl_ = 0;
    
    void EmitNALU(const std::vector<uint8_t>& nalu, 
                 const std::function<void(const std::vector<uint8_t>&)>& emit_func);
};

class H265Packetizer {
public:
    explicit H265Packetizer(uint16_t mtu = 1500);
    ~H265Packetizer() = default;
    
    bool Packetize(const std::vector<uint8_t>& h265_frame, 
                  std::vector<std::vector<uint8_t>>* rtp_packets);
    
    // Configure options
    void WithDONL(bool value);
    void WithSkipAggregation(bool value);
    void WithSequencer(std::shared_ptr<Sequencer> sequencer);
    void WithTimestamp(uint32_t timestamp);
    void WithSSRC(uint32_t ssrc);
    void WithPayloadType(uint8_t payload_type);
    
private:
    H265Payloader payloader_;
    uint16_t mtu_;
    std::shared_ptr<Sequencer> sequencer_;
    uint32_t timestamp_ = 0;
    uint32_t ssrc_ = 0;
    uint8_t payload_type_ = 0;
};

} // namespace rtp

#endif // H265_PACKETIZER_H_