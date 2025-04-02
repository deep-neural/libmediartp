#ifndef RTP_AV1_PACKETIZER_H_
#define RTP_AV1_PACKETIZER_H_

#include <cstdint>
#include <vector>
#include "av1_packet.h"

namespace rtp {

// AV1Packetizer packetizes AV1 frames into RTP packets
class AV1Packetizer {
public:
    explicit AV1Packetizer(size_t mtu);
    ~AV1Packetizer() = default;
    
    // Packetize converts an AV1 OBU stream into RTP packets
    bool Packetize(const std::vector<uint8_t>& frame, 
                  std::vector<std::vector<uint8_t>>* out_packets);
                  
private:
    // Measure the maximum write size for a payload with leb128 encoding added
    size_t ComputeWriteSize(size_t want_to_write, size_t can_write) const;
    
    // Calculate the size of a leb128 encoded value and whether it's at edge of size change
    void Leb128Size(size_t value, size_t* size, bool* is_at_edge) const;
    
    // Append an OBU to the current set of payloads
    std::tuple<std::vector<std::vector<uint8_t>>, int> AppendOBUPayload(
        std::vector<std::vector<uint8_t>> payloads,
        const std::vector<uint8_t>& obu_payload,
        bool is_new_video_sequence,
        bool is_last,
        bool start_with_new_packet,
        int mtu,
        int current_obu_count);
        
    size_t mtu_;
};

} // namespace rtp

#endif // RTP_AV1_PACKETIZER_H_