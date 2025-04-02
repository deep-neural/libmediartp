#include "opus_packetizer.h"

namespace rtp {

OPUSPacketizer::OPUSPacketizer(size_t mtu) 
    : mtu_(mtu), sequencer_(std::make_shared<RandomSequencer>()) {
    // Initialize default header
    header_.version = 2;
    header_.padding = false;
    header_.extension = false;
    header_.marker = false;
    header_.payload_type = 0;  // Default, should be set by user
    header_.sequence_number = 0;
    header_.timestamp = 0;
    header_.ssrc = 0;
}

void OPUSPacketizer::SetRTPHeader(const Header& header) {
    header_ = header;
}

void OPUSPacketizer::SetSequencer(std::shared_ptr<Sequencer> sequencer) {
    sequencer_ = sequencer;
}

bool OPUSPacketizer::Packetize(const std::vector<uint8_t>& opus_frame,
                             std::vector<std::vector<uint8_t>>* rtp_packets) {
    if (opus_frame.empty() || !rtp_packets) {
        return false;
    }
    
    rtp_packets->clear();
    
    // For Opus, we simply copy the entire frame as the payload
    // This implementation follows the Go example where we don't fragment Opus packets
    std::vector<uint8_t> payload = opus_frame;
    
    // Check if the payload fits in our MTU
    Header packet_header = header_;
    packet_header.sequence_number = sequencer_->NextSequenceNumber();
    
    Packet rtp_packet;
    rtp_packet.header = packet_header;
    rtp_packet.payload = payload;
    
    // Set marker bit for the last (and only) packet
    rtp_packet.header.marker = true;
    
    // Serialize the packet
    std::vector<uint8_t> serialized_packet = rtp_packet.Packetize();
    
    // Check if it exceeds MTU
    if (serialized_packet.size() > mtu_) {
        // In a real implementation, you might want to fragment the Opus frame
        // However, the Go example doesn't fragment, so we'll just report failure
        return false;
    }
    
    // Add the packet to the output
    rtp_packets->push_back(std::move(serialized_packet));
    
    return true;
}

} // namespace rtp