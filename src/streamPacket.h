#pragma once
#include <vector>
#include <cstdint>

struct StreamPacket {
    std::vector<uint8_t> data;
    uint64_t dts;
    uint64_t pts;
};
