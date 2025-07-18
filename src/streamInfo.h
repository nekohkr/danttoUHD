#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "mpd.h"
#include "aacEncoder.h"
#include "mp4Processor.h"

struct StreamInfo {
    uint32_t idx;
    uint32_t transportSessionId;
    std::string fileName;
    std::vector<uint8_t> initMP4;

    uint32_t srcIpAddr;
    uint32_t dstIpAddr;
    uint16_t dstPort;

    std::string language;

    bool encrypted{ false };
    ContentType contentType{ ContentType::UNKNOWN };
    bool hasInitToi{ false };
    uint32_t initToi{ 0 };

    MP4ConfigParser::MP4Config mp4Config;
};