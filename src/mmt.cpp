#include "mmt.h"

namespace atsc3 {

bool MmtSignalingMessagePayload::unpack(Common::ReadStream& stream) {
    uint8_t uint8 = stream.get8U();
    fragmentationIndicator = static_cast<MmtFragmentationIndicator>((uint8 & 0b11000000) >> 6);
    reserved = (uint8 & 0b00111100) >> 2;
    lengthExtensionFlag = (uint8 & 0b00000010) >> 1;
    aggregationFlag = uint8 & 0b00000001;

    fragmentationCounter = stream.get8U();
    return true;
}

bool MmtSignalingMessagePayloadEntry::unpack(Common::ReadStream& stream, bool aggregationFlag, bool lengthExtensionFlag) {
    if (aggregationFlag) {
        msgLength = lengthExtensionFlag ? stream.getBe32U() : stream.getBe16U();
        if (stream.leftBytes() < msgLength) {
            return false;
        }

        payload.resize(msgLength);
        stream.read(payload.data(), msgLength);
    }
    else {
        payload.resize(stream.leftBytes());
        stream.read(payload.data(), stream.leftBytes());
    }

    return true;
}

bool MmtMpuDataUnit::unpack(Common::ReadStream& stream, MmtMpuFragmentType mpuFramgentType, bool timedFlag, bool aggregateFlag) {
    if (aggregateFlag == 1) {
        dataUnitLength = stream.getBe16U();
    }

    if (mpuFramgentType == MmtMpuFragmentType::Mfu) {
        if (timedFlag) {
            movieFragmentSequenceNumber = stream.getBe32U();
            sampleNumber = stream.getBe32U();
            offset = stream.getBe32U();
            priority = stream.get8U();
            dependencyCounter = stream.get8U();
        }
        else {
            if (aggregateFlag) {
                itemId = stream.getBe32U();
            }
        }
    }

    if (aggregateFlag == 0) {
        payload.resize(stream.leftBytes());
        stream.read(payload.data(), stream.leftBytes());
    }
    else {
        uint32_t payloadLength = dataUnitLength - timedFlag ? (4 * 3 + 1 * 2) : 4;
        payload.resize(payloadLength);
        stream.read(payload.data(), payloadLength);
    }

    return true;
}

bool MmtMpu::unpack(Common::ReadStream& stream) {
    length = stream.getBe16U();

    uint8_t uint8 = stream.get8U();
    mpuFragmentType = static_cast<MmtMpuFragmentType>((uint8 & 0b11110000) >> 4);
    timedFlag = (uint8 & 0b00001000) >> 3;
    fragmentationIndicator = static_cast<MmtFragmentationIndicator>((uint8 & 0b00000110) >> 1);
    aggregationFlag = uint8 & 0b00000001;

    fragmentCounter = stream.get8U();
    mpuSequenceNumber = stream.getBe32U();
    return true;
}

bool MmtpHeaderExtention::unpack(Common::ReadStream& stream) {
    type = stream.getBe16U();
    length = stream.getBe16U();
    value.resize(length);
    stream.read(value.data(), length);
    return true;
}

bool Mmtp::unpack(Common::ReadStream& stream) {
    uint8_t uint8 = stream.get8U();
    version = (uint8 & 0b11000000) >> 6;
    packetCounterFlag = (uint8 & 0b00100000) >> 5;
    fecType = (uint8 & 0b00011000) >> 3;

    if (version == 0) {
        reserved0 = (uint8 & 0b00000100) >> 2;
        extensionFlag = (uint8 & 0b00000010) >> 1;
        rapFlag = uint8 & 0b00000001;

        uint8 = stream.get8U();
        reserved1 = (uint8 & 0b11000000) >> 6;
        type = static_cast<MmtpType>(uint8 & 0b00111111);
    }
    else {
        extensionFlag = (uint8 & 0b00000100) >> 2;
        rapFlag = (uint8 & 0b00000010) >> 1;
        qosClassifierFlag = uint8 & 0b00000001;

        uint8 = stream.get8U();
        flowIdentifierFlag = (uint8 & 0b10000000) >> 7;
        flowExtensionFlag = (uint8 & 0b01000000) >> 6;
        compressFlag = (uint8 & 0b00100000) >> 5;
        indicatorFlag = (uint8 & 0b00010000) >> 4;
        type = static_cast<MmtpType>(uint8 & 0b00001111);
    }

    packetId = stream.getBe16U();
    timestamp = stream.getBe32U();
    packetSequenceNumber = stream.getBe32U();

    if (packetCounterFlag) {
        packetCounter = stream.getBe32U();
    }

    if (version == 1) {
        uint8 = stream.get8U();
        reserved0 = (uint8 & 0b10000000) >> 7;
        typeOfBitrate = (uint8 & 0b01100000) >> 5;
        delaySensitivity = (uint8 & 0b00011100) >> 2;
        transmissionPriority = uint8 & 0b00000011;

        uint8 = stream.get8U();
        transmissionPriority |= (uint8 & 0b10000000) >> 7;
        flowLabel = uint8 & 0b01111111;
    }

    if (extensionFlag) {
        headerExtention.unpack(stream);
    }

    uint32_t payloadLength = static_cast<uint32_t>(stream.leftBytes() - (fecType == 1 ? 4 : 0));
    payload.resize(payloadLength);
    stream.read(payload.data(), payloadLength);

    if (fecType == 1) {
        sourceFecPayloadId = stream.getBe32U();
    }
    
    return true;
}

bool MmtGeneralLocationInfo::unpack(Common::ReadStream& stream) {
    locationType = stream.get8U();
    if (locationType == 0x00) {
        packetId = stream.getBe16U();
    }
    else if (locationType == 0x01) {
        ipv4SrcAddr = stream.getBe32U();
        ipv4DstAddr = stream.getBe32U();
        dstPort = stream.getBe16U();
        packetId = stream.getBe16U();
    }
    else if (locationType == 0x02) {
        stream.read(ipv6SrcAddr, 16);
        stream.read(ipv6DstAddr, 16);
        dstPort = stream.getBe16U();
        packetId = stream.getBe16U();
    }
    else if (locationType == 0x03) {
        networkId = stream.getBe16U();
        mpeg2TransportStreamId = stream.getBe16U();

        uint16_t uint16 = stream.getBe16U();
        reserved = (uint16 & 0b1110000000000000) >> 13;
        mpeg2Pid = uint16 & 0b0001111111111111;
    }
    else if (locationType == 0x04) {
        stream.read(ipv6SrcAddr, 16);
        stream.read(ipv6DstAddr, 16);
        dstPort = stream.getBe16U();

        uint16_t uint16 = stream.getBe16U();
        reserved = (uint16 & 0b1110000000000000) >> 13;
        mpeg2Pid = uint16 & 0b0001111111111111;
    }
    else if (locationType == 0x05) {
        uint8_t urlLength = stream.get8U();
        url.resize(urlLength);
        stream.read(url.data(), urlLength);
    }
    else if (locationType == 0x06) {
        uint16_t length = stream.getBe16U();
        byte.resize(length);
        stream.read(byte.data(), length);
    }
    else if (locationType == 0x07) {
    }
    else if (locationType == 0x08) {
        messageId = stream.getBe16U();
    }
    else if (locationType == 0x09) {
        packetId = stream.getBe16U();
        messageId = stream.getBe16U();
    }
    else if (locationType == 0x0A) {
        ipv4SrcAddr = stream.getBe32U();
        ipv4DstAddr = stream.getBe32U();
        dstPort = stream.getBe16U();
        packetId = stream.getBe16U();
        messageId = stream.getBe16U();
    }
    else if (locationType == 0x0B) {
        stream.read(ipv6SrcAddr, 16);
        stream.read(ipv6DstAddr, 16);
        dstPort = stream.getBe16U();
        packetId = stream.getBe16U();
        messageId = stream.getBe16U();
    }
    else if (locationType == 0x0C) {
        ipv4SrcAddr = stream.getBe32U();
        ipv4DstAddr = stream.getBe32U();
        dstPort = stream.getBe16U();

        uint16_t uint16 = stream.getBe16U();
        reserved = (uint16 & 0b1110000000000000) >> 13;
        mpeg2Pid = uint16 & 0b0001111111111111;
    }

    return true;
}

bool MmtMMTHSample::unpack(Common::ReadStream& stream) {
    sequnceNumber = stream.getBe32U();
    trackRefIndex = stream.get8U();
    movieFramgentSequenceNumber = stream.get32U();
    smapleNumber = stream.get32U();
    priority = stream.get8U();
    dependencyCounter = stream.get8U();
    offset = stream.getBe32U();
    length = stream.getBe32U();
    boxSize = stream.getBe32U();
    boxType = stream.getBe32U();

    uint8_t uint8 = stream.get8U();
    multilayerFlag = (uint8 & 0b10000000) >> 7;

    if (multilayerFlag) {
        uint8 = stream.get8U();
        dependencyId = (uint8 & 0b11100000) >> 5;
        depthFlag = (uint8 & 0b00010000) >> 4;
        reserved1 = uint8 & 0b00001111;

        uint8 = stream.get8U();
        temporalId = (uint8 & 0b11100000) >> 5;
        reserved2 = (uint8 & 0b00010000) >> 4;
        qualityId = uint8 & 0b00001111;

        uint16_t uint16 = stream.getBe16U();
        priorityId = (uint16 & 0b1111110000000000) >> 10;
        viewId = uint16 & 0b0000001111111111;
    }
    else {
        uint16_t uint16 = stream.getBe16U();
        layerId = (uint8 & 0b1111110000000000) >> 10;
        temporalId = (uint8 & 0b0000001110000000) >> 7;
        reserved3 = uint8 & 0b0000000001111111;
    }

    return true;
}

}