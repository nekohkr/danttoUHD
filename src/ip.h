#pragma once
#include <cstdint>
#include <WS2tcpip.h>
#include <string>

namespace Common {

inline uint32_t ipToUint(const std::string& ip) {
    uint32_t addr = 0;
    inet_pton(AF_INET, ip.c_str(), &addr);
    return addr;
}

}