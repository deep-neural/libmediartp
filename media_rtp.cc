#include "media_rtp.h"

// Include all the necessary internal headers
#include "av1_packetizer.h"
#include "av1_depacketizer.h"
#include "h264_packetizer.h"
#include "h264_depacketizer.h"
#include "h265_packetizer.h"
#include "h265_depacketizer.h"
#include "opus_packetizer.h"
#include "opus_depacketizer.h"
#include "vp8_packetizer.h"
#include "vp8_depacketizer.h"
#include "vp9_packetizer.h"
#include "vp9_depacketizer.h"

namespace media {

namespace internal {

// Implementation classes that hide the details of each codec's packetizer/depacketizer

class PacketizerImpl {
public:
    virtual ~PacketizerImpl() = default;
    virtual bool Packetize(const std::vector<uint8_t>& frame, 
                        std::vector<std::vector<uint8_t>>* rtp_packets) = 0;
    virtual void SetSSRC(uint32_t ssrc) = 0;
    virtual void SetPayloadType(uint8_t payload_type) = 0;
    virtual void SetTimestamp(uint32_t timestamp) = 0;
};

class DepacketizerImpl {
public:
    virtual ~DepacketizerImpl() = default;
    virtual bool Depacketize(const std::vector<uint8_t>& rtp_packet, 
                           std::vector<uint8_t>* out_frame) = 0;
    virtual bool IsFrameStart(const std::vector<uint8_t>& rtp_packet) = 0;
    virtual bool IsFrameEnd(const std::vector<uint8_t>& rtp_packet) = 0;
};

// Implementation for AV1
class AV1PacketizerImpl : public PacketizerImpl {
public:
    explicit AV1PacketizerImpl(uint16_t mtu) : packetizer_(mtu) {}
    ~AV1PacketizerImpl() override = default;

    bool Packetize(const std::vector<uint8_t>& frame, 
                std::vector<std::vector<uint8_t>>* rtp_packets) override {
        return packetizer_.Packetize(frame, rtp_packets);
    }

    void SetSSRC(uint32_t ssrc) override {
        // Not implemented in AV1Packetizer
    }

    void SetPayloadType(uint8_t payload_type) override {
        // Not implemented in AV1Packetizer
    }

    void SetTimestamp(uint32_t timestamp) override {
        // Not implemented in AV1Packetizer
    }

private:
    rtp::AV1Packetizer packetizer_;
};

class AV1DepacketizerImpl : public DepacketizerImpl {
public:
    AV1DepacketizerImpl() = default;
    ~AV1DepacketizerImpl() override = default;

    bool Depacketize(const std::vector<uint8_t>& rtp_packet, 
                   std::vector<uint8_t>* out_frame) override {
        return depacketizer_.Depacketize(rtp_packet, out_frame);
    }

    bool IsFrameStart(const std::vector<uint8_t>& rtp_packet) override {
        return depacketizer_.IsPartitionHead(rtp_packet);
    }

    bool IsFrameEnd(const std::vector<uint8_t>& rtp_packet) override {
        // AV1 doesn't have a specific IsPartitionTail method
        return false;
    }

private:
    rtp::AV1Depacketizer depacketizer_;
};

// Implementation for H264
class H264PacketizerImpl : public PacketizerImpl {
public:
    explicit H264PacketizerImpl(uint16_t mtu) : packetizer_(mtu) {}
    ~H264PacketizerImpl() override = default;

    bool Packetize(const std::vector<uint8_t>& frame, 
                std::vector<std::vector<uint8_t>>* rtp_packets) override {
        return packetizer_.Packetize(frame, rtp_packets);
    }

    void SetSSRC(uint32_t ssrc) override {
        // Not directly exposed in H264Packetizer
    }

    void SetPayloadType(uint8_t payload_type) override {
        // Not directly exposed in H264Packetizer
    }

    void SetTimestamp(uint32_t timestamp) override {
        // Not directly exposed in H264Packetizer
    }

    void EnableStapA(bool enable) {
        if (enable) {
            packetizer_.EnableStapA();
        } else {
            packetizer_.DisableStapA();
        }
    }

private:
    rtp::H264Packetizer packetizer_;
};

class H264DepacketizerImpl : public DepacketizerImpl {
public:
    H264DepacketizerImpl() = default;
    ~H264DepacketizerImpl() override = default;

    bool Depacketize(const std::vector<uint8_t>& rtp_packet, 
                   std::vector<uint8_t>* out_frame) override {
        return depacketizer_.Depacketize(rtp_packet, out_frame);
    }

    bool IsFrameStart(const std::vector<uint8_t>& rtp_packet) override {
        return depacketizer_.IsPartitionHead(rtp_packet);
    }

    bool IsFrameEnd(const std::vector<uint8_t>& rtp_packet) override {
        // Use the marker bit to determine if it's the end
        bool marker = true; // Extract marker bit from RTP packet
        return depacketizer_.IsPartitionTail(marker, rtp_packet);
    }

private:
    rtp::H264Depacketizer depacketizer_;
};

// Implementation for H265
class H265PacketizerImpl : public PacketizerImpl {
public:
    explicit H265PacketizerImpl(uint16_t mtu) : packetizer_(mtu) {}
    ~H265PacketizerImpl() override = default;

    bool Packetize(const std::vector<uint8_t>& frame, 
                std::vector<std::vector<uint8_t>>* rtp_packets) override {
        return packetizer_.Packetize(frame, rtp_packets);
    }

    void SetSSRC(uint32_t ssrc) override {
        packetizer_.WithSSRC(ssrc);
    }

    void SetPayloadType(uint8_t payload_type) override {
        packetizer_.WithPayloadType(payload_type);
    }

    void SetTimestamp(uint32_t timestamp) override {
        packetizer_.WithTimestamp(timestamp);
    }

    void SetDONL(bool enable) {
        packetizer_.WithDONL(enable);
    }

    void SetSkipAggregation(bool value) {
        packetizer_.WithSkipAggregation(value);
    }

private:
    rtp::H265Packetizer packetizer_;
};

class H265DepacketizerImpl : public DepacketizerImpl {
public:
    H265DepacketizerImpl() = default;
    ~H265DepacketizerImpl() override = default;

    bool Depacketize(const std::vector<uint8_t>& rtp_packet, 
                   std::vector<uint8_t>* out_frame) override {
        return depacketizer_.Depacketize(rtp_packet, out_frame);
    }

    bool IsFrameStart(const std::vector<uint8_t>& rtp_packet) override {
        return depacketizer_.IsPartitionHead(rtp_packet);
    }

    bool IsFrameEnd(const std::vector<uint8_t>& rtp_packet) override {
        bool marker = true; // Extract marker bit from RTP packet
        return depacketizer_.IsPartitionTail(marker, rtp_packet);
    }

    void SetDONL(bool enable) {
        depacketizer_.WithDONL(enable);
    }

private:
    rtp::H265Depacketizer depacketizer_;
};

// Implementation for OPUS
class OPUSPacketizerImpl : public PacketizerImpl {
public:
    explicit OPUSPacketizerImpl(uint16_t mtu) : packetizer_(mtu) {}
    ~OPUSPacketizerImpl() override = default;

    bool Packetize(const std::vector<uint8_t>& frame, 
                std::vector<std::vector<uint8_t>>* rtp_packets) override {
        return packetizer_.Packetize(frame, rtp_packets);
    }

    void SetSSRC(uint32_t ssrc) override {
        rtp::Header header = packetizer_.GetRTPHeader();
        header.ssrc = ssrc;
        packetizer_.SetRTPHeader(header);
    }

    void SetPayloadType(uint8_t payload_type) override {
        rtp::Header header = packetizer_.GetRTPHeader();
        header.payload_type = payload_type;
        packetizer_.SetRTPHeader(header);
    }

    void SetTimestamp(uint32_t timestamp) override {
        rtp::Header header = packetizer_.GetRTPHeader();
        header.timestamp = timestamp;
        packetizer_.SetRTPHeader(header);
    }

private:
    rtp::OPUSPacketizer packetizer_;
};

class OPUSDepacketizerImpl : public DepacketizerImpl {
public:
    OPUSDepacketizerImpl() = default;
    ~OPUSDepacketizerImpl() override = default;

    bool Depacketize(const std::vector<uint8_t>& rtp_packet, 
                   std::vector<uint8_t>* out_frame) override {
        return depacketizer_.Depacketize(rtp_packet, out_frame);
    }

    bool IsFrameStart(const std::vector<uint8_t>& rtp_packet) override {
        return depacketizer_.IsPartitionHead(rtp_packet);
    }

    bool IsFrameEnd(const std::vector<uint8_t>& rtp_packet) override {
        bool marker = true; // Extract marker bit from RTP packet
        return depacketizer_.IsPartitionTail(marker, rtp_packet);
    }

private:
    rtp::OPUSDepacketizer depacketizer_;
};

// Implementation for VP8
class VP8PacketizerImpl : public PacketizerImpl {
public:
    explicit VP8PacketizerImpl(uint16_t mtu) : packetizer_(mtu) {}
    ~VP8PacketizerImpl() override = default;

    bool Packetize(const std::vector<uint8_t>& frame, 
                std::vector<std::vector<uint8_t>>* rtp_packets) override {
        return packetizer_.Packetize(frame, rtp_packets);
    }

    void SetSSRC(uint32_t ssrc) override {
        packetizer_.SetSSRC(ssrc);
    }

    void SetPayloadType(uint8_t payload_type) override {
        packetizer_.SetPayloadType(payload_type);
    }

    void SetTimestamp(uint32_t timestamp) override {
        packetizer_.SetTimestamp(timestamp);
    }

    void EnablePictureID(bool enable) {
        packetizer_.EnablePictureID(enable);
    }

private:
    rtp::VP8Packetizer packetizer_;
};

class VP8DepacketizerImpl : public DepacketizerImpl {
public:
    VP8DepacketizerImpl() = default;
    ~VP8DepacketizerImpl() override = default;

    bool Depacketize(const std::vector<uint8_t>& rtp_packet, 
                   std::vector<uint8_t>* out_frame) override {
        return depacketizer_.Depacketize(rtp_packet, out_frame);
    }

    bool IsFrameStart(const std::vector<uint8_t>& rtp_packet) override {
        return depacketizer_.IsPartitionHead(rtp_packet);
    }

    bool IsFrameEnd(const std::vector<uint8_t>& rtp_packet) override {
        bool marker = true; // Extract marker bit from RTP packet
        return depacketizer_.IsPartitionTail(marker, rtp_packet);
    }

private:
    rtp::VP8Depacketizer depacketizer_;
};

// Implementation for VP9
class VP9PacketizerImpl : public PacketizerImpl {
public:
    explicit VP9PacketizerImpl(uint16_t mtu) : packetizer_(mtu) {}
    ~VP9PacketizerImpl() override = default;

    bool Packetize(const std::vector<uint8_t>& frame, 
                std::vector<std::vector<uint8_t>>* rtp_packets) override {
        return packetizer_.Packetize(frame, rtp_packets);
    }

    void SetSSRC(uint32_t ssrc) override {
        // Not directly exposed in VP9Packetizer
    }

    void SetPayloadType(uint8_t payload_type) override {
        // Not directly exposed in VP9Packetizer
    }

    void SetTimestamp(uint32_t timestamp) override {
        // Not directly exposed in VP9Packetizer
    }

    void SetInitialPictureID(uint16_t id) {
        packetizer_.SetInitialPictureID(id);
    }

    void SetFlexibleMode(bool enable) {
        packetizer_.SetFlexibleMode(enable);
    }

private:
    rtp::VP9Packetizer packetizer_;
};

class VP9DepacketizerImpl : public DepacketizerImpl {
public:
    VP9DepacketizerImpl() = default;
    ~VP9DepacketizerImpl() override = default;

    bool Depacketize(const std::vector<uint8_t>& rtp_packet, 
                   std::vector<uint8_t>* out_frame) override {
        return depacketizer_.Depacketize(rtp_packet, out_frame);
    }

    bool IsFrameStart(const std::vector<uint8_t>& rtp_packet) override {
        return depacketizer_.IsPartitionHead(rtp_packet);
    }

    bool IsFrameEnd(const std::vector<uint8_t>& rtp_packet) override {
        bool marker = true; // Extract marker bit from RTP packet
        return depacketizer_.IsPartitionTail(marker, rtp_packet);
    }

private:
    rtp::VP9Depacketizer depacketizer_;
};

} // namespace internal

//----------------------------------------
// RTPPacketizer Implementation
//----------------------------------------

RTPPacketizer::RTPPacketizer(Codec codec, uint16_t mtu) {
    switch (codec) {
        case Codec::AV1:
            impl_ = std::make_unique<internal::AV1PacketizerImpl>(mtu);
            break;
        case Codec::H264:
            impl_ = std::make_unique<internal::H264PacketizerImpl>(mtu);
            break;
        case Codec::H265:
            impl_ = std::make_unique<internal::H265PacketizerImpl>(mtu);
            break;
        case Codec::OPUS:
            impl_ = std::make_unique<internal::OPUSPacketizerImpl>(mtu);
            break;
        case Codec::VP8:
            impl_ = std::make_unique<internal::VP8PacketizerImpl>(mtu);
            break;
        case Codec::VP9:
            impl_ = std::make_unique<internal::VP9PacketizerImpl>(mtu);
            break;
    }
}

RTPPacketizer::~RTPPacketizer() = default;

bool RTPPacketizer::Packetize(const std::vector<uint8_t>& frame, 
                             std::vector<std::vector<uint8_t>>* rtp_packets) {
    return impl_->Packetize(frame, rtp_packets);
}

void RTPPacketizer::SetSSRC(uint32_t ssrc) {
    impl_->SetSSRC(ssrc);
}

void RTPPacketizer::SetPayloadType(uint8_t payload_type) {
    impl_->SetPayloadType(payload_type);
}

void RTPPacketizer::SetTimestamp(uint32_t timestamp) {
    impl_->SetTimestamp(timestamp);
}

void RTPPacketizer::EnableStapA(bool enable) {
    auto* h264_impl = dynamic_cast<internal::H264PacketizerImpl*>(impl_.get());
    if (h264_impl) {
        h264_impl->EnableStapA(enable);
    }
}

void RTPPacketizer::SetDONL(bool enable) {
    auto* h265_impl = dynamic_cast<internal::H265PacketizerImpl*>(impl_.get());
    if (h265_impl) {
        h265_impl->SetDONL(enable);
    }
}

void RTPPacketizer::EnablePictureID(bool enable) {
    auto* vp8_impl = dynamic_cast<internal::VP8PacketizerImpl*>(impl_.get());
    if (vp8_impl) {
        vp8_impl->EnablePictureID(enable);
    }
}

void RTPPacketizer::SetInitialPictureID(uint16_t id) {
    auto* vp9_impl = dynamic_cast<internal::VP9PacketizerImpl*>(impl_.get());
    if (vp9_impl) {
        vp9_impl->SetInitialPictureID(id);
    }
}

void RTPPacketizer::SetFlexibleMode(bool enable) {
    auto* vp9_impl = dynamic_cast<internal::VP9PacketizerImpl*>(impl_.get());
    if (vp9_impl) {
        vp9_impl->SetFlexibleMode(enable);
    }
}

//----------------------------------------
// RTPDepacketizer Implementation
//----------------------------------------

RTPDepacketizer::RTPDepacketizer(Codec codec) {
    switch (codec) {
        case Codec::AV1:
            impl_ = std::make_unique<internal::AV1DepacketizerImpl>();
            break;
        case Codec::H264:
            impl_ = std::make_unique<internal::H264DepacketizerImpl>();
            break;
        case Codec::H265:
            impl_ = std::make_unique<internal::H265DepacketizerImpl>();
            break;
        case Codec::OPUS:
            impl_ = std::make_unique<internal::OPUSDepacketizerImpl>();
            break;
        case Codec::VP8:
            impl_ = std::make_unique<internal::VP8DepacketizerImpl>();
            break;
        case Codec::VP9:
            impl_ = std::make_unique<internal::VP9DepacketizerImpl>();
            break;
    }
}

RTPDepacketizer::~RTPDepacketizer() = default;

bool RTPDepacketizer::Depacketize(const std::vector<uint8_t>& rtp_packet, 
                                 std::vector<uint8_t>* out_frame) {
    return impl_->Depacketize(rtp_packet, out_frame);
}

bool RTPDepacketizer::IsFrameStart(const std::vector<uint8_t>& rtp_packet) {
    return impl_->IsFrameStart(rtp_packet);
}

bool RTPDepacketizer::IsFrameEnd(const std::vector<uint8_t>& rtp_packet) {
    return impl_->IsFrameEnd(rtp_packet);
}

void RTPDepacketizer::SetDONL(bool enable) {
    auto* h265_impl = dynamic_cast<internal::H265DepacketizerImpl*>(impl_.get());
    if (h265_impl) {
        h265_impl->SetDONL(enable);
    }
}

//----------------------------------------
// Version Information
//----------------------------------------

Version GetVersion() {
    return {1, 0, 0}; // Initial version
}

} // namespace