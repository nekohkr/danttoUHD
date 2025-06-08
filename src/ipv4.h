#pragma once
#include <cstdint>

namespace Common {
    class ReadStream;
}

class IPv4Header {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint8_t version{};
    uint8_t ihl{};
    uint32_t tos{};
    uint16_t length{};
    uint16_t id{};
    uint8_t flags{};
    uint16_t fragmentOffset{};
    uint8_t ttl{};
    uint8_t protocol{};
    uint16_t headerChecksum{};
    uint32_t srcIpAddr{};
    uint32_t dstIpAddr{};


};