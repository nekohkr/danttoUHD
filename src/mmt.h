#pragma once
#include <cstdint>
#include <vector>
#include "stream.h"
#include <list>

namespace atsc3 {

enum class MmtpType : uint8_t {
    Mpu = 0x00,
    GenericObject = 0x01,
    SignallingMessage = 0x02,
    RepairSymbol = 0x03,
};

enum class MmtMpuFragmentType : uint8_t {
    MpuMetadata = 0,
    MovieFragmentMetadata = 1,
    Mfu = 2,
};

enum class MmtFragmentationIndicator : uint8_t {
    NotFragmented = 0,
    FirstFragment = 1,
    MiddleFragment = 2,
    LastFragment = 3,
};

constexpr int32_t makeAssetType(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return (a << 24) | (b << 16) | (c << 8) | d;
}

namespace MmtAssetType {

constexpr int32_t hev1 = makeAssetType('h', 'e', 'v', '1');
constexpr int32_t mp4a = makeAssetType('m', 'p', '4', 'a');
constexpr int32_t mhm1 = makeAssetType('m', 'h', 'm', '1');
constexpr int32_t stpp = makeAssetType('s', 't', 'p', 'p');
constexpr int32_t aapp = makeAssetType('a', 'a', 'p', 'p');

}

namespace MmtMessageId {
    constexpr uint16_t PaMessage = 0x0000;
    constexpr uint16_t MpiMessageStart = 0x0001;
    constexpr uint16_t MpiMessageEnd = 0x0010;
    constexpr uint16_t MptMessageStart = 0x0011;
    constexpr uint16_t MptMessageEnd = 0x0020;

    constexpr bool isMpiMessage(uint16_t value) {
        return value >= MpiMessageStart && value <= MpiMessageEnd;
    }
    constexpr bool isMptMessage(uint16_t value) {
        return value >= MptMessageStart && value <= MptMessageEnd;
    }
}

class MmtMMTHSample {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint32_t sequnceNumber;
    uint32_t trackRefIndex;
    uint32_t movieFramgentSequenceNumber;
    uint32_t smapleNumber;
    uint16_t priority;
    uint16_t dependencyCounter;
    uint16_t offset;
    uint32_t length;
    uint32_t boxSize;
    uint32_t boxType;
    uint32_t multilayerFlag;
    uint8_t dependencyId;
    bool depthFlag;
    uint8_t reserved1;
    uint8_t temporalId;
    uint8_t reserved2;
    uint8_t qualityId;
    uint8_t priorityId;
    uint16_t viewId;
    uint8_t layerId;
    uint8_t reserved3;
};

class MmtGeneralLocationInfo {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint8_t locationType;
    uint16_t packetId;
    uint32_t ipv4SrcAddr;
    uint32_t ipv4DstAddr;
    uint16_t dstPort;
    uint8_t ipv6SrcAddr[16];
    uint8_t ipv6DstAddr[16];
    uint16_t networkId;
    uint16_t mpeg2TransportStreamId;
    uint8_t reserved;
    uint16_t mpeg2Pid;
    uint8_t urlLength;
    std::string url;
    std::vector<uint8_t> byte;
    uint16_t messageId;
};

class MmtSignalingMessagePayloadEntry {
public:
    bool unpack(Common::ReadStream& stream, bool aggregationFlag, bool lengthExtensionFlag);

public:
    uint32_t msgLength;
    std::vector<uint8_t> payload;
};

class MmtSignalingMessagePayload {
public:
    bool unpack(Common::ReadStream& stream);

public:
    MmtFragmentationIndicator fragmentationIndicator;
    uint8_t reserved;
    bool aggregationFlag;
    uint8_t fragmentationCounter;
    bool lengthExtensionFlag;

};

class MmtMpuDataUnit {
public:
    bool unpack(Common::ReadStream& stream, MmtMpuFragmentType mpuFragmentType, bool timedFlag, bool aggregateFlag);

public:
    uint16_t dataUnitLength;
    uint32_t movieFragmentSequenceNumber;
    uint32_t sampleNumber;
    uint32_t offset;
    uint8_t priority;
    uint8_t dependencyCounter;
    uint32_t itemId;
    std::vector<uint8_t> payload;

};

class MmtMpu {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint16_t length;
    MmtMpuFragmentType mpuFragmentType;
    bool timedFlag;
    MmtFragmentationIndicator fragmentationIndicator;
    uint8_t aggregationFlag;
    uint8_t fragmentCounter;
    uint32_t mpuSequenceNumber;

};

class MmtpHeaderExtention {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint16_t type;
    uint16_t length;
    std::vector<uint8_t> value;

};

class Mmtp {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint8_t version;
    bool packetCounterFlag;
    uint8_t fecType;
    uint8_t reserved0;
    bool extensionFlag;
    bool rapFlag;
    uint8_t reserved1;
    bool compressFlag;
    bool indicatorFlag;
    MmtpType type;
    uint16_t packetId;
    uint32_t packetSequenceNumber;
    uint32_t timestamp;
    uint32_t packetCounter;
    uint32_t sourceFecPayloadId;
    MmtpHeaderExtention headerExtention;
    bool qosClassifierFlag;
    bool flowIdentifierFlag;
    bool flowExtensionFlag;
    bool reliabilityFlag;
    uint8_t typeOfBitrate;
    uint8_t delaySensitivity;
    uint8_t transmissionPriority;
    uint8_t flowLabel;
    std::vector<uint8_t> payload;

};

}