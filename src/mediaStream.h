#pragma once
#include <cstdint>
#include "mp4Processor.h"

namespace atsc3 {

enum class StreamType {
    VIDEO,
    AUDIO,
    SUBTITLE,
    UNKNOWN,
};
enum class CodecType {
    MPEG1,
    MPEG2,
    MPEGH3D,
    HEVC,
    AAC,
    AAC_LATM,
    UNKNOWN,
};

class MediaStream {
public:;
    virtual ~MediaStream() = default;
    virtual StreamType getStreamType() const { return StreamType::UNKNOWN; }
    virtual CodecType getCodecType() const { return CodecType::UNKNOWN; }

public:
    uint16_t idx{0};
    uint16_t packetId{0};
    struct MP4CodecConfig mp4CodecConfig;

};

}