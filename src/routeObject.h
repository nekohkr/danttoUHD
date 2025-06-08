#pragma once
#include <cstdint>
#include <vector>

enum class ContentType {
    VIDEO,
    AUDIO,
    SUBTITLE,
    UNKNOWN,
};

struct RouteObject {
public:
    uint32_t transportSessionId{0};
    uint32_t count{ 0 };
    uint16_t sbn{ 0xFFFF };

    std::vector<uint8_t> buffer;
    bool readyToBuffer{ false };

    std::vector<uint8_t> initMP4;
    bool encrypted{ false };
    bool inited{ false };
    ContentType contentType{ ContentType::UNKNOWN };
    bool hasInitToi{ false };
    uint32_t initToi{ 0 };
    std::vector<uint8_t> configNalUnits;
    uint32_t timescale;
};