#include "vp9_packetizer.h"
#include <chrono>

namespace rtp {

VP9Packetizer::VP9Packetizer(uint16_t mtu) 
    : mtu_(mtu),
      vp9Header_(std::make_unique<VP9Header>()) {
    // Seed the random generator with current time
    uint32_t seed = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    randomGenerator_.seed(seed);
}

VP9Packetizer::~VP9Packetizer() = default;

uint16_t VP9Packetizer::generateRandomPictureID() {
    std::uniform_int_distribution<uint16_t> dist(0, 0x7FFF);
    return dist(randomGenerator_);
}

bool VP9Packetizer::Packetize(const std::vector<uint8_t>& vp9Frame, std::vector<std::vector<uint8_t>>* rtpPackets) {
    if (vp9Frame.empty() || !rtpPackets) {
        return false;
    }
    
    // Initialize if needed
    if (!initialized_) {
        pictureID_ = generateRandomPictureID() & 0x7FFF;
        initialized_ = true;
    }
    
    // Generate payloads
    std::vector<std::vector<uint8_t>> payloads;
    if (flexibleMode_) {
        payloads = payloadFlexible(vp9Frame);
    } else {
        payloads = payloadNonFlexible(vp9Frame);
    }
    
    // Increment picture ID for next frame
    pictureID_++;
    if (pictureID_ >= 0x8000) {
        pictureID_ = 0;
    }
    
    // Return generated packets
    rtpPackets->swap(payloads);
    return !rtpPackets->empty();
}

std::vector<std::vector<uint8_t>> VP9Packetizer::payloadFlexible(const std::vector<uint8_t>& payload) {
    /*
     * Flexible mode (F=1)
     *        0 1 2 3 4 5 6 7
     *       +-+-+-+-+-+-+-+-+
     *       |I|P|L|F|B|E|V|Z| (REQUIRED)
     *       +-+-+-+-+-+-+-+-+
     *  I:   |M| PICTURE ID  | (REQUIRED)
     *       +-+-+-+-+-+-+-+-+
     *  M:   | EXTENDED PID  | (RECOMMENDED)
     *       +-+-+-+-+-+-+-+-+
     */
    
    const int headerSize = 3; // 1 byte VP9 descriptor + 2 bytes for picture ID
    const int maxFragmentSize = mtu_ - headerSize;
    int payloadDataRemaining = payload.size();
    int payloadDataIndex = 0;
    std::vector<std::vector<uint8_t>> payloads;
    
    if (minInt(maxFragmentSize, payloadDataRemaining) <= 0) {
        return payloads; // Return empty vector
    }
    
    while (payloadDataRemaining > 0) {
        int currentFragmentSize = minInt(maxFragmentSize, payloadDataRemaining);
        std::vector<uint8_t> out(headerSize + currentFragmentSize);
        
        // Set VP9 descriptor
        out[0] = 0x90; // F=1, I=1
        if (payloadDataIndex == 0) {
            out[0] |= 0x08; // B=1 (beginning of frame)
        }
        if (payloadDataRemaining == currentFragmentSize) {
            out[0] |= 0x04; // E=1 (end of frame)
        }
        
        // Set picture ID (always using 15-bit PictureID)
        out[1] = static_cast<uint8_t>(pictureID_ >> 8) | 0x80; // M=1 (extended picture ID)
        out[2] = static_cast<uint8_t>(pictureID_ & 0xFF);
        
        // Copy payload fragment
        std::copy(
            payload.begin() + payloadDataIndex, 
            payload.begin() + payloadDataIndex + currentFragmentSize, 
            out.begin() + headerSize
        );
        
        payloads.push_back(out);
        payloadDataRemaining -= currentFragmentSize;
        payloadDataIndex += currentFragmentSize;
    }
    
    return payloads;
}

std::vector<std::vector<uint8_t>> VP9Packetizer::payloadNonFlexible(const std::vector<uint8_t>& payload) {
    /*
     * Non-flexible mode (F=0)
     *        0 1 2 3 4 5 6 7
     *       +-+-+-+-+-+-+-+-+
     *       |I|P|L|F|B|E|V|Z| (REQUIRED)
     *       +-+-+-+-+-+-+-+-+
     *  I:   |M| PICTURE ID  | (REQUIRED)
     *       +-+-+-+-+-+-+-+-+
     *  M:   | EXTENDED PID  | (RECOMMENDED)
     *       +-+-+-+-+-+-+-+-+
     *  L:   |  T  |U|  S  |D| (REQUIRED)
     *       +-+-+-+-+-+-+-+-+
     *       |     TL0PICIDX | (REQUIRED)
     *       +-+-+-+-+-+-+-+-+
     */
    
    // Try to extract VP9 frame header information
    bool isKeyFrame = false;
    if (payload.size() >= 1 && vp9Header_->Unmarshal(payload)) {
        isKeyFrame = !vp9Header_->NonKeyFrame;
    }
    
    // Determine header size based on options
    int headerSize = 1; // VP9 descriptor byte
    headerSize += 2;    // Picture ID (16-bit)
    headerSize += 2;    // Layer indices and TL0PICIDX
    
    const int maxFragmentSize = mtu_ - headerSize;
    int payloadDataRemaining = payload.size();
    int payloadDataIndex = 0;
    std::vector<std::vector<uint8_t>> payloads;
    
    if (minInt(maxFragmentSize, payloadDataRemaining) <= 0) {
        return payloads; // Return empty vector
    }
    
    // Static temporal layer with no spatial scalability
    const uint8_t temporalID = 0;
    const uint8_t spatialID = 0;
    const bool layerSync = false;
    
    while (payloadDataRemaining > 0) {
        int currentFragmentSize = minInt(maxFragmentSize, payloadDataRemaining);
        std::vector<uint8_t> out(headerSize + currentFragmentSize);
        
        // Set VP9 descriptor
        uint8_t descriptor = 0x00;
        
        // Set required bits
        descriptor |= 0x80; // I=1 (PictureID present)
        descriptor |= 0x20; // L=1 (Layer indices present)
        if (!isKeyFrame) {
            descriptor |= 0x40; // P=1 (Inter-picture predicted)
        }
        
        // Set fragment indicator bits
        if (payloadDataIndex == 0) {
            descriptor |= 0x08; // B=1 (beginning of frame)
        }
        if (payloadDataRemaining == currentFragmentSize) {
            descriptor |= 0x04; // E=1 (end of frame)
        }
        
        out[0] = descriptor;
        
        // Set picture ID (always using 15-bit PictureID)
        out[1] = static_cast<uint8_t>(pictureID_ >> 8) | 0x80; // M=1 (extended picture ID)
        out[2] = static_cast<uint8_t>(pictureID_ & 0xFF);
        
        // Set layer indices
        out[3] = (temporalID << 5) | (spatialID << 1);
        if (layerSync) {
            out[3] |= 0x10; // U=1 (Switching up point)
        }
        
        // Set temporal layer zero index
        static uint8_t tl0PicIdx = 0;
        out[4] = tl0PicIdx;
        if (temporalID == 0) {
            tl0PicIdx++;
        }
        
        // Copy payload fragment
        std::copy(
            payload.begin() + payloadDataIndex, 
            payload.begin() + payloadDataIndex + currentFragmentSize, 
            out.begin() + headerSize
        );
        
        payloads.push_back(out);
        payloadDataRemaining -= currentFragmentSize;
        payloadDataIndex += currentFragmentSize;
    }
    
    return payloads;
}

} // namespace rtp