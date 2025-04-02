#ifndef VP8_PACKETIZER_H_
#define VP8_PACKETIZER_H_

#include <cstdint>
#include <vector>
#include <memory>
#include "rtp_packet.h"
#include "vp8_packet.h"

namespace rtp {

// VP8Packetizer packetizes VP8 frames into RTP packets
class VP8Packetizer {
public:
    explicit VP8Packetizer(uint16_t mtu);
    ~VP8Packetizer() = default;
    
    // Packetize a VP8 frame into multiple RTP packets
    bool Packetize(const std::vector<uint8_t>& vp8_frame, 
                   std::vector<std::vector<uint8_t>>* rtp_packets);
    
    // Enable/disable picture ID
    void EnablePictureID(bool enable) { enable_picture_id_ = enable; }
    
    // Set the SSRC for all generated packets
    void SetSSRC(uint32_t ssrc) { ssrc_ = ssrc; }
    
    // Set the payload type for all generated packets
    void SetPayloadType(uint8_t payload_type) { payload_type_ = payload_type; }
    
    // Set the timestamp for the next frame
    void SetTimestamp(uint32_t timestamp) { timestamp_ = timestamp; }
    
    // Reset the picture ID counter
    void ResetPictureID() { picture_id_ = 0; }

private:
    // Helper functions
    template<typename T>
    T minValue(T a, T b) const { return a < b ? a : b; }
    
    uint16_t mtu_;                   // Maximum transfer unit
    bool enable_picture_id_ = false; // Enable picture ID
    uint16_t picture_id_ = 0;        // Current picture ID (0-0x7FFF)
    uint32_t ssrc_ = 0;              // Synchronization source
    uint8_t payload_type_ = 96;      // Default RTP payload type for VP8
    uint32_t timestamp_ = 0;         // Current timestamp
    std::unique_ptr<Sequencer> sequencer_; // Sequence number generator
};

} // namespace rtp

#endif // VP8_PACKETIZER_H_