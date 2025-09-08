#pragma once
#include <cstdint>
#include "stream.h"

namespace atsc3 {

enum OFI {
    NoExtensionMode = 0b00,
    ShortExtensionMode = 0b01,
    LongExtensionMode = 0b10,
    MixedExtensionMode = 0b11,
};

class Atsc3BasebandPacket {
public:
    bool unpack(Common::ReadStream& s);


    class BaseField {
    public:
        bool unpack(Common::ReadStream& s);

        uint8_t     mode{ 0 };
        uint32_t    pointer{ 0 };
        uint8_t     ofi{ 0 };
    };

    class OptionalField {
    public:
        bool unpack(Common::ReadStream& s, const BaseField& baseField);

        uint8_t     extType{ 0 };
        uint32_t    extLen{ 0 };
    };


    BaseField baseField;
    OptionalField optionalField;
    std::vector<uint8_t> extension;
    std::vector<uint8_t> payload;
};

}