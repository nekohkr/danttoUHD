#pragma once
#include <cstdint>
#include <vector>

struct RouteObject {
public:
    uint32_t transportSessionId{0};
    uint32_t count{ 0 };

    std::vector<uint8_t> buffer;
    bool readyToBuffer{ false };
};