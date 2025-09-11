#include "atsc3.h"
#include "decompress.h"

namespace atsc3 {

bool Atsc3LowLevelSignaling::unpack(Common::ReadStream& stream) {
    tableId = stream.get8U();
    groupId = stream.get8U();
    groupCount = stream.get8U();
    tableVersion = stream.get8U();

    std::vector<uint8_t> compressed;
    compressed.resize(stream.leftBytes());
    stream.read(compressed.data(), compressed.size());

    auto decompressed = gzipInflate(compressed);
    if (!decompressed) {
        return false;
    }

    payload = decompressed.value();
    return true;
}


UnpackResult Atsc3Alp::unpack(Common::ReadStream& s) {
    uint16_t uint16 = s.getBe16U();
    packetType = (uint16 & 0b1110000000000000) >> 13;
    payloadConfiguration = static_cast<bool>((uint16 & 0b0001000000000000) >> 12);

    if (payloadConfiguration == 0) {
        headerMode = (uint16 & 0b0000100000000000) >> 11;
        length = uint16 & 0b0000011111111111;

        if (headerMode == 1) {
            uint8_t uint8 = s.get8U();
            uint8_t headerLengthMSB = (uint8 & 0b11111000) >> 3;
            uint8_t headerReserved = (uint8 & 0b00000100) >> 2;
            uint8_t sif = (uint8 & 0b00000010) >> 1;
            uint8_t hef = uint8 & 0b00000001;

            if (sif == 1) {
                uint8_t subStreamIdentification = s.get8U();
            }
        }
    }
    else {
        uint8_t segmentationConcatenation = (uint16 & 0b0000100000000000) >> 11;
        length = uint16 & 0b0000011111111111;

        if (segmentationConcatenation == 0) {
            uint8_t uint8 = s.get8U();
            uint8_t segmentSequenceNumber = (uint8 & 0b11111000) >> 3;
            uint8_t lastSegmentIndicator = (uint8 & 0b00000100) >> 2;
            uint8_t sif = (uint8 & 0b00000010) >> 1;
            uint8_t hef = uint8 & 0b00000001;

            if (sif == 1) {
                uint8_t subStreamIdentification = s.get8U();
            }

            // todo
        }
        else {
            // todo
        }
    }

    if (s.leftBytes() < length) {
        return UnpackResult::NotEnoughData;
    }

    payload.resize(length);
    s.read(payload.data(), length);
    return UnpackResult::Success;
}


bool Atsc3BasebandPacket::unpack(Common::ReadStream& s) {
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

bool Atsc3BasebandPacket::BaseField::unpack(Common::ReadStream& s) {
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

bool Atsc3BasebandPacket::OptionalField::unpack(Common::ReadStream& s, const BaseField& baseField)
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