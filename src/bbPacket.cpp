#include "bbPacket.h"

namespace ATSC3 {

bool BasebandPacket::unpack(Common::ReadStream& s) {
    uint8_t ext_type = 0;
    uint32_t ext_len = 0;

    if (!baseField.unpack(s)) {
        return false;
    }

    if (!optionalField.unpack(s, baseField)) {
        return false;
    }

    if (optionalField.extLen > 0) {
        if (s.leftBytes() < optionalField.extLen) {
            return false;
        }
        extension.resize(optionalField.extLen);
        s.read(extension.data(), optionalField.extLen);
    }

    if (s.leftBytes() > 0) {
        payload.resize(s.leftBytes());
        s.read(payload.data(), s.leftBytes());
    }

    return true;
}

bool BasebandPacket::BaseField::unpack(Common::ReadStream& s) {
    if (s.leftBytes() < 2) {
        return false;
    }

    uint8_t uint8 = s.get8U();
    mode = (uint8 & 0b10000000) >> 7;
    pointer = uint8 & 0b01111111;

    if (mode == 1) {
        uint8 = s.get8U();
        pointer |= (uint8 & 0b11111100) << 5;
        ofi = uint8 & 0b00000011;
    }

    return true;
}

bool BasebandPacket::OptionalField::unpack(Common::ReadStream& s, const BaseField& baseField)
{
    if (baseField.mode == 0) {
        return true;
    }

    OFI ofi = static_cast<OFI>(baseField.ofi);
    if (ofi == OFI::NoExtensionMode) {
        return true;
    }

    uint8_t uint8 = s.get8U();
    extType = (uint8 & 0b11100000) >> 5;
    extLen = uint8 & 0b00011111;

    if (ofi == OFI::LongExtensionMode) {
        uint8_t uint8 = s.get8U();
        extLen |= uint8 << 5;
    }

    return true;
}

}
