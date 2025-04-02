#ifndef OPUS_PACKETIZER_H_
#define OPUS_PACKETIZER_H_

#include "opus_packet.h"
#include "rtp_packet.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace rtp {

class OPUSPacketizer {
public:
    // Constructor with maximum RTP packet size
    explicit OPUSPacketizer(size_t mtu);
    ~OPUSPacketizer() = default;

    // Packetize an Opus frame into one or more RTP packets
    bool Packetize(const std::vector<uint8_t>& opus_frame, 
                  std::vector<std::vector<uint8_t>>* rtp_packets);

    // Configure the RTP header for the packetizer
    void SetRTPHeader(const Header& header);
    
    // Get the current RTP header
    const Header& GetRTPHeader() const { return header_; }
    
    // Set the sequencer used for generating sequence numbers
    void SetSequencer(std::shared_ptr<Sequencer> sequencer);

private:
    Header header_;                           // RTP header template
    size_t mtu_;                              // Maximum transmission unit (packet size)
    std::shared_ptr<Sequencer> sequencer_;    // Sequence number generator
};

} // namespace rtp

#endif // OPUS_PACKETIZER_H_