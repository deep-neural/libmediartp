#include "vp9_packet.h"
#include <stdexcept>

namespace rtp {

// Error constants
const char* kErrNilPacketVP9 = "nil packet";
const char* kErrShortPacketVP9 = "packet too short";
const char* kErrTooManySpatialLayers = "too many spatial layers";
const char* kErrTooManyPDiff = "too many PDiff elements";

// Bit reading utilities implementation
bool hasSpace(const std::vector<uint8_t>& buf, int pos, int n) {
    return n <= ((buf.size() * 8) - pos);
}

bool readFlag(const std::vector<uint8_t>& buf, int* pos) {
    if (!hasSpace(buf, *pos, 1)) {
        throw std::runtime_error("not enough bits");
    }
    return readFlagUnsafe(buf, pos);
}

bool readFlagUnsafe(const std::vector<uint8_t>& buf, int* pos) {
    bool b = (buf[*pos >> 0x03] >> (7 - (*pos & 0x07))) & 0x01;
    (*pos)++;
    return b == 1;
}

uint64_t readBits(const std::vector<uint8_t>& buf, int* pos, int n) {
    if (!hasSpace(buf, *pos, n)) {
        throw std::runtime_error("not enough bits");
    }
    return readBitsUnsafe(buf, pos, n);
}

uint64_t readBitsUnsafe(const std::vector<uint8_t>& buf, int* pos, int n) {
    int res = 8 - (*pos & 0x07);
    if (n < res) {
        uint64_t bits = (buf[*pos >> 0x03] >> (res - n)) & ((1 << n) - 1);
        *pos += n;
        return bits;
    }

    uint64_t bits = buf[*pos >> 0x03] & ((1 << res) - 1);
    *pos += res;
    n -= res;

    while (n >= 8) {
        bits = (bits << 8) | buf[*pos >> 0x03];
        *pos += 8;
        n -= 8;
    }

    if (n > 0) {
        bits = (bits << n) | (buf[*pos >> 0x03] >> (8 - n));
        *pos += n;
    }

    return bits;
}

// VP9Packet implementation
bool VP9Packet::IsPartitionHead(const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
        return false;
    }
    return (payload[0] & 0x08) != 0; // B flag
}

bool VP9Packet::Unmarshal(const std::vector<uint8_t>& packet, std::vector<uint8_t>* payload) {
    if (packet.empty()) {
        return false;
    }
    if (packet.size() < 1) {
        return false;
    }

    I = (packet[0] & 0x80) != 0;
    P = (packet[0] & 0x40) != 0;
    L = (packet[0] & 0x20) != 0;
    F = (packet[0] & 0x10) != 0;
    B = (packet[0] & 0x08) != 0;
    E = (packet[0] & 0x04) != 0;
    V = (packet[0] & 0x02) != 0;
    Z = (packet[0] & 0x01) != 0;

    int pos = 1;
    try {
        if (I) {
            pos = parsePictureID(packet, pos);
        }

        if (L) {
            pos = parseLayerInfo(packet, pos);
        }

        if (F && P) {
            pos = parseRefIndices(packet, pos);
        }

        if (V) {
            pos = parseSSData(packet, pos);
        }

        // Copy payload
        if (pos < static_cast<int>(packet.size())) {
            payload->assign(packet.begin() + pos, packet.end());
        } else {
            payload->clear();
        }
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

int VP9Packet::parsePictureID(const std::vector<uint8_t>& packet, int pos) {
    if (packet.size() <= static_cast<size_t>(pos)) {
        throw std::runtime_error(kErrShortPacketVP9);
    }

    PictureID = packet[pos] & 0x7F;
    if ((packet[pos] & 0x80) != 0) {
        pos++;
        if (packet.size() <= static_cast<size_t>(pos)) {
            throw std::runtime_error(kErrShortPacketVP9);
        }
        PictureID = (PictureID << 8) | packet[pos];
    }
    pos++;

    return pos;
}

int VP9Packet::parseLayerInfo(const std::vector<uint8_t>& packet, int pos) {
    pos = parseLayerInfoCommon(packet, pos);

    if (F) {
        return pos;
    }

    return parseLayerInfoNonFlexibleMode(packet, pos);
}

int VP9Packet::parseLayerInfoCommon(const std::vector<uint8_t>& packet, int pos) {
    if (packet.size() <= static_cast<size_t>(pos)) {
        throw std::runtime_error(kErrShortPacketVP9);
    }

    TID = packet[pos] >> 5;
    U = (packet[pos] & 0x10) != 0;
    SID = (packet[pos] >> 1) & 0x7;
    D = (packet[pos] & 0x01) != 0;

    if (SID >= kMaxSpatialLayers) {
        throw std::runtime_error(kErrTooManySpatialLayers);
    }

    pos++;
    return pos;
}

int VP9Packet::parseLayerInfoNonFlexibleMode(const std::vector<uint8_t>& packet, int pos) {
    if (packet.size() <= static_cast<size_t>(pos)) {
        throw std::runtime_error(kErrShortPacketVP9);
    }

    TL0PICIDX = packet[pos];
    pos++;

    return pos;
}

int VP9Packet::parseRefIndices(const std::vector<uint8_t>& packet, int pos) {
    PDiff.clear();
    
    while (true) {
        if (packet.size() <= static_cast<size_t>(pos)) {
            throw std::runtime_error(kErrShortPacketVP9);
        }
        PDiff.push_back(packet[pos] >> 1);
        if ((packet[pos] & 0x01) == 0) {
            break;
        }
        if (PDiff.size() >= kMaxVP9RefPics) {
            throw std::runtime_error(kErrTooManyPDiff);
        }
        pos++;
    }
    pos++;

    return pos;
}

int VP9Packet::parseSSData(const std::vector<uint8_t>& packet, int pos) {
    if (packet.size() <= static_cast<size_t>(pos)) {
        throw std::runtime_error(kErrShortPacketVP9);
    }

    NS = packet[pos] >> 5;
    Y = (packet[pos] & 0x10) != 0;
    G = (packet[pos] & 0x08) != 0;
    pos++;

    int ns = NS + 1;
    NG = 0;

    if (Y) {
        Width.resize(ns);
        Height.resize(ns);
        
        for (int i = 0; i < ns; i++) {
            if (packet.size() <= static_cast<size_t>(pos + 3)) {
                throw std::runtime_error(kErrShortPacketVP9);
            }

            Width[i] = (packet[pos] << 8) | packet[pos + 1];
            pos += 2;
            Height[i] = (packet[pos] << 8) | packet[pos + 1];
            pos += 2;
        }
    }

    if (G) {
        if (packet.size() <= static_cast<size_t>(pos)) {
            throw std::runtime_error(kErrShortPacketVP9);
        }

        NG = packet[pos];
        pos++;
    }

    PGTID.clear();
    PGU.clear();
    PGPDiff.clear();

    for (int i = 0; i < NG; i++) {
        if (packet.size() <= static_cast<size_t>(pos)) {
            throw std::runtime_error(kErrShortPacketVP9);
        }

        PGTID.push_back(packet[pos] >> 5);
        PGU.push_back((packet[pos] & 0x10) != 0);
        uint8_t R = (packet[pos] >> 2) & 0x3;
        pos++;

        PGPDiff.push_back({});

        if (packet.size() <= static_cast<size_t>(pos + R - 1)) {
            throw std::runtime_error(kErrShortPacketVP9);
        }

        for (int j = 0; j < R; j++) {
            PGPDiff[i].push_back(packet[pos]);
            pos++;
        }
    }

    return pos;
}

// VP9Header implementation
bool VP9Header::Unmarshal(const std::vector<uint8_t>& buf) {
    int pos = 0;
    
    try {
        if (!hasSpace(buf, pos, 4)) {
            return false;
        }

        uint64_t frameMarker = readBitsUnsafe(buf, &pos, 2);
        if (frameMarker != 2) {
            return false;
        }

        uint8_t profileLowBit = static_cast<uint8_t>(readBitsUnsafe(buf, &pos, 1));
        uint8_t profileHighBit = static_cast<uint8_t>(readBitsUnsafe(buf, &pos, 1));
        Profile = (profileHighBit << 1) + profileLowBit;

        if (Profile == 3) {
            if (!hasSpace(buf, pos, 1)) {
                return false;
            }
            pos++;
        }

        ShowExistingFrame = readFlag(buf, &pos);

        if (ShowExistingFrame) {
            FrameToShowMapIdx = static_cast<uint8_t>(readBits(buf, &pos, 3));
            return true;
        }

        if (!hasSpace(buf, pos, 3)) {
            return false;
        }

        NonKeyFrame = readFlagUnsafe(buf, &pos);
        ShowFrame = readFlagUnsafe(buf, &pos);
        ErrorResilientMode = readFlagUnsafe(buf, &pos);

        if (!NonKeyFrame) {
            if (!hasSpace(buf, pos, 24)) {
                return false;
            }

            uint8_t frameSyncByte0 = static_cast<uint8_t>(readBitsUnsafe(buf, &pos, 8));
            if (frameSyncByte0 != 0x49) {
                return false;
            }

            uint8_t frameSyncByte1 = static_cast<uint8_t>(readBitsUnsafe(buf, &pos, 8));
            if (frameSyncByte1 != 0x83) {
                return false;
            }

            uint8_t frameSyncByte2 = static_cast<uint8_t>(readBitsUnsafe(buf, &pos, 8));
            if (frameSyncByte2 != 0x42) {
                return false;
            }

            ColorConfigData = std::make_unique<ColorConfig>();
            if (!ColorConfigData->Unmarshal(Profile, buf, &pos)) {
                return false;
            }

            FrameSizeData = std::make_unique<FrameSize>();
            if (!FrameSizeData->Unmarshal(buf, &pos)) {
                return false;
            }
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

uint16_t VP9Header::Width() const {
    if (!FrameSizeData) {
        return 0;
    }
    return FrameSizeData->FrameWidthMinus1 + 1;
}

uint16_t VP9Header::Height() const {
    if (!FrameSizeData) {
        return 0;
    }
    return FrameSizeData->FrameHeightMinus1 + 1;
}

// ColorConfig implementation
bool VP9Header::ColorConfig::Unmarshal(uint8_t profile, const std::vector<uint8_t>& buf, int* pos) {
    try {
        if (profile >= 2) {
            TenOrTwelveBit = readFlag(buf, pos);
            BitDepth = TenOrTwelveBit ? 12 : 10;
        } else {
            BitDepth = 8;
        }

        ColorSpace = static_cast<uint8_t>(readBits(buf, pos, 3));

        if (ColorSpace != 7) {
            ColorRange = readFlag(buf, pos);

            if (profile == 1 || profile == 3) {
                if (!hasSpace(buf, *pos, 3)) {
                    return false;
                }
                SubsamplingX = readFlagUnsafe(buf, pos);
                SubsamplingY = readFlagUnsafe(buf, pos);
                (*pos)++;
            } else {
                SubsamplingX = true;
                SubsamplingY = true;
            }
        } else {
            ColorRange = true;

            if (profile == 1 || profile == 3) {
                SubsamplingX = false;
                SubsamplingY = false;

                if (!hasSpace(buf, *pos, 1)) {
                    return false;
                }
                (*pos)++;
            }
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// FrameSize implementation
bool VP9Header::FrameSize::Unmarshal(const std::vector<uint8_t>& buf, int* pos) {
    try {
        if (!hasSpace(buf, *pos, 32)) {
            return false;
        }

        FrameWidthMinus1 = static_cast<uint16_t>(readBitsUnsafe(buf, pos, 16));
        FrameHeightMinus1 = static_cast<uint16_t>(readBitsUnsafe(buf, pos, 16));

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace rtp