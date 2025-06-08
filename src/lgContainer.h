#pragma once
#include <functional>

struct LgContainer {
    uint32_t syncWord;
    bool errorMode;
    bool error;
    uint8_t plpId;
    uint16_t size;
    uint8_t cc;
    uint8_t t_mode;
    uint64_t time_value;

    std::vector<uint8_t> payload;
};

class LgContainerUnpacker {
public:
    using UnpackCallback = std::function<void(const LgContainer&)>;

    void addBuffer(const std::vector<uint8_t>& data);
    void unpack(const UnpackCallback& callback);
    void clear();

private:
    std::vector<uint8_t> buffer;
};