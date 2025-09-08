#pragma once
#include <cstdint>
#include <string>
#include "stream.h"

namespace atsc3 {

enum class Atsc3AlpPacketType {
    IPv4 = 0x0,
    RESERVED_001 = 0x1,
    COMPRESSED_IP = 0x2,
    RESERVED_011 = 0x3,
    LINK_LAYER_SIGNALLING = 0x4,
    RESERVED_101 = 0x5,
    PACKET_TYPE_EXTENSION = 0x6,
    MPEG2_TS = 0x7
};

enum class Atsc3ServiceCategory : uint8_t {
    LinearAVService = 0x1,
    LinearAudioOnlyService = 0x2,
    AppBasedService = 0x3,
    EsgService = 0x4,
    EasService = 0x5,
};

class Atsc3LowLevelSignaling {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint8_t tableId;
    uint8_t groupId;
    uint8_t groupCount;
    uint8_t tableVersion;
    std::string payload;
};

enum class UnpackResult {
    Success = 0,
    NotEnoughData = 1,
    InvalidData = 2,
    Error = 3
};

class Atsc3Alp {
public:
    UnpackResult unpack(Common::ReadStream& stream);

    uint8_t packetType{ 0 };
    bool payloadConfiguration{ false };
    bool headerMode{ false };
    uint16_t length{ 0 };
    bool segmentationConcatenation;
    std::vector<uint8_t> payload;
};

}