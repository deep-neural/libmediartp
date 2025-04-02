cmake_minimum_required(VERSION 3.10)

project(mediartp)

# Add includes
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}  # Base project directory
    ${CMAKE_CURRENT_SOURCE_DIR}/packet
    ${CMAKE_CURRENT_SOURCE_DIR}/depacketizer
    ${CMAKE_CURRENT_SOURCE_DIR}/packetizer
)

add_library(mediartp STATIC
    # Packet implementations
    packet/rtp_packet.cc
    packet/rtp_packet.h
    packet/vp9_packet.cc
    packet/vp9_packet.h
    packet/vp8_packet.cc
    packet/vp8_packet.h
    packet/opus_packet.cc
    packet/opus_packet.h
    packet/h265_packet.cc
    packet/h265_packet.h
    packet/h264_packet.cc
    packet/h264_packet.h
    packet/av1_packet.cc
    packet/av1_packet.h

    # Depacketizers
    depacketizer/vp9_depacketizer.cc
    depacketizer/vp9_depacketizer.h
    depacketizer/vp8_depacketizer.cc
    depacketizer/vp8_depacketizer.h
    depacketizer/opus_depacketizer.cc
    depacketizer/opus_depacketizer.h
    depacketizer/h265_depacketizer.cc
    depacketizer/h265_depacketizer.h
    depacketizer/h264_depacketizer.cc
    depacketizer/h264_depacketizer.h
    depacketizer/av1_depacketizer.cc
    depacketizer/av1_depacketizer.h

    # Packetizers
    packetizer/vp9_packetizer.cc
    packetizer/vp9_packetizer.h
    packetizer/vp8_packetizer.cc
    packetizer/vp8_packetizer.h
    packetizer/opus_packetizer.cc
    packetizer/opus_packetizer.h
    packetizer/h265_packetizer.cc
    packetizer/h265_packetizer.h
    packetizer/h264_packetizer.cc
    packetizer/h264_packetizer.h
    packetizer/av1_packetizer.cc
    packetizer/av1_packetizer.h

    # library
    media_rtp.cc
    media_rtp.h
)

# Set the output name for the static library
set_target_properties(mediartp PROPERTIES OUTPUT_NAME "mediartp")

# Specify include directories
target_include_directories(mediartp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})