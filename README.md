<h1 align="center">
  <a href="https://github.com/deep-neural/libmediartp"><img src="./.github/logo.jpg" alt="libmediartp" height="150px"></a>
  <br>
  libMediaRTP
  <br>
</h1>
<h4 align="center">A pure C/C++ implementation for building media streams and RTP packet processing</h4>
<p align="center">
    <a href="https://github.com/deep-neural/libmediartp"><img src="https://img.shields.io/badge/libmediartp-C/C++-blue.svg?longCache=true" alt="libmediartp" /></a>
  <a href="https://datatracker.ietf.org/doc/html/rfc3550"><img src="https://img.shields.io/static/v1?label=RFC&message=3550&color=brightgreen" /></a>
  <a href="https://datatracker.ietf.org/doc/html/rfc3551"><img src="https://img.shields.io/static/v1?label=RFC&message=3551&color=brightgreen" /></a>
  <a href="https://datatracker.ietf.org/doc/html/rfc4585"><img src="https://img.shields.io/static/v1?label=RFC&message=4585&color=brightgreen" /></a>
  <a href="https://datatracker.ietf.org/doc/html/rfc5761"><img src="https://img.shields.io/static/v1?label=RFC&message=5761&color=brightgreen" /></a>
  <br>
    <a href="https://github.com/deep-neural/libmediartp"><img src="https://img.shields.io/static/v1?label=Build&message=Documentation&color=brightgreen" /></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-5865F2.svg" alt="License: MIT" /></a>
</p>
<br>

### New Release

libmediartp v1.0.0 has been released! See the [release notes](https://github.com/libmediartp/libmediartp/wiki/Release-libmediartp@v1.0.0) to learn about new features, enhancements, and breaking changes.

If you aren't ready to upgrade yet, check the [tags](https://github.com/libmediartp/libmediartp/tags) for previous stable releases.

We appreciate your feedback! Feel free to open GitHub issues or submit changes to stay updated in development and connect with the maintainers.

-----

### Usage

libmediartp is distributed as a pure C/C++ library. To integrate it into your project, ensure you have a compatible C/C++ compiler and the necessary build tools (e.g., Make, CMake). Clone the repository and link against the library in your build system.

## Simple API
<table>
<tr>
<th> Packetizer </th>
<th> Depacketizer </th>
</tr>
<tr>
<td>

```cpp
// Create a packetizer
media::RTPPacketizer packetizer(mediartp::Codec::AV1, 1400);

// Configure packetizer
packetizer.SetSSRC(12345);
packetizer.SetPayloadType(96);
packetizer.SetTimestamp(90000);
    
// Input AV1 frame
std::vector<uint8_t> av1_frame;

// Packetize the frame
std::vector<std::vector<uint8_t>> rtp_packets;
if (packetizer.Packetize(av1_frame, &rtp_packets)) {

}
```

</td>
<td>

```cpp
// Create a depacketizer for AV1
media::RTPDepacketizer depacketizer(mediartp::Codec::AV1);

// Process each RTP packet
std::vector<uint8_t> reconstructed_frame;
for (const auto& packet : rtp_packets) {
    depacketizer.Depacketize(packet, &reconstructed_frame);
}
```

</td>
</tr>
</table>

**[Example Applications](examples/README.md)** contain code samples demonstrating common use cases with libmediartp.

**[API Documentation](https://libmediartp.example/docs)** provides a comprehensive reference of our Public APIs.

Now go build something amazing! Here are some ideas to spark your creativity:
* Create low-latency media broadcasting solutions with seamless protocol integration.
* Build mesh streaming networks for distributed media delivery.
* Implement real-time audio/video processing pipelines with precise packet timing.
* Design reliable media streaming for challenging network environments.

## Building

See [BUILDING.md](https://github.com/danielv4/libmediartp/blob/master/BUILDING.md) for building instructions.

### Features

#### Media Stream Management
* Comprehensive RTP (Real-time Transport Protocol) implementation supporting various media codecs.
* Flexible packet framing and sequencing for audio, video, and data streams.
* Advanced jitter buffer implementation with configurable parameters.

#### Protocol Integration
* Seamless compatibility with QUIC, WebTransport, and WebRTC protocols.
* Protocol-agnostic design allowing deployment across different transport layers.
* Support for RTCP (RTP Control Protocol) feedback mechanisms and statistics.

#### Packet Processing
* Efficient RTP packet creation, parsing, and manipulation.
* Header extension support for advanced media applications.
* Optimized packet scheduling and priority management.

#### Media Synchronization
* Precise timing mechanisms for synchronized multi-stream playback.
* SRTP/SRTCP integration for secure media transfer.
* NTP/PTP clock synchronization support.

#### Pure C/C++
* Written entirely in C/C++ with no external dependencies beyond standard libraries.
* Wide platform support: Windows, macOS, Linux, FreeBSD, and more.
* Optimized for high performance with fast builds and a comprehensive test suite.
* Easily integrated into existing projects using common build systems.

### Contributing

Check out the [contributing guide](https://github.com/libmediartp/libmediartp/wiki/Contributing) to join the team of dedicated contributors making this project possible.

### License

MIT License - see [LICENSE](LICENSE) for full text
