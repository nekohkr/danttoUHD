#include "mmtTable.h"

namespace atsc3 {

bool MmtTable::unpack(Common::ReadStream& stream) {
    tableId = stream.get8U();
    version = stream.get8U();
    length = stream.getBe16U();
    return true;
}

bool MmtMpiTable::unpack(Common::ReadStream& stream) {
    if (!MmtTable::unpack(stream)) {
        return false;
    }

    uint8_t uint8 = stream.get8U();
    reserved0 = (uint8 & 0b11110000) >> 4;
    piMode = (uint8 & 0b00001100) >> 2;
    reserved1 = uint8 & 0b00000011;

    mpitDescriptorsLength = stream.getBe16U();

    return true;
}

bool MmtMpTable::unpack(Common::ReadStream& stream) {
    if (!MmtTable::unpack(stream)) {
        return false;
    }

    uint8_t uint8 = stream.get8U();
    reserved0 = (uint8 & 0b11111100) >> 2;
    mpTableMode = uint8 & 0b00000011;

    if (tableId == 0x20 || tableId == 0x11) {
        mmtPackageIdLength = stream.get8U();
        mmtPackageId.resize(mmtPackageIdLength);
        stream.read(mmtPackageId.data(), mmtPackageIdLength);

        mpTableDescriptorsLength = stream.getBe16U();
        if (mpTableDescriptorsLength) {
            // TODO
            stream.skip(mpTableDescriptorsLength);
        }
    }

    assetCount = stream.get8U();
    for (int i = 0; i < assetCount; i++) {
        Asset asset;
        if (!asset.unpack(stream)) {
            return false;
        }

        assets.push_back(std::move(asset));
    }

    return true;
}

bool MmtMpTable::IdentifierMapping::unpack(Common::ReadStream& stream) {
    identifierType = static_cast<MmtMpTable::IdentifierType>(stream.get8U());

    if (identifierType == IdentifierType::AssetId) {
        assetId.assetIdScheme = stream.getBe32U();
        assetId.assetIdLength = stream.getBe32U();
        assetId.assetId.resize(assetId.assetIdLength);
        stream.read(assetId.assetId.data(), assetId.assetIdLength);
    }
    else if (identifierType == IdentifierType::Urls) {
        urlCount = stream.getBe16U();
        for (int i2 = 0; i2 < urlCount; i2++) {
            uint16_t urlLength = stream.getBe16U();
            std::string url;
            url.resize(urlLength);
            stream.read(url.data(), urlLength);
            urls.push_back(std::move(url));
        }
    }
    else if (identifierType == IdentifierType::Regex) {
        regexLength = stream.getBe16U();
        regex.resize(regexLength);
        stream.read(regex.data(), regexLength);
    }
    else if (identifierType == IdentifierType::RespresentationId) {
        representationIdLength = stream.getBe16U();
        representationId.resize(representationIdLength);
        stream.read(representationId.data(), representationIdLength);
    }
    else {
        privateLength = stream.getBe16U();
        privateByte.resize(privateLength);
        stream.read(privateByte.data(), privateLength);
    }

    return true;
}

bool MmtMpTable::Asset::unpack(Common::ReadStream& stream) {
    if (!identifierMapping.unpack(stream)) {
        return false;
    }

    assetType = stream.getBe32U();

    uint8_t uint8 = stream.get8U();
    reserved0 = (uint8 & 0b11111000) >> 3;
    assetModificationFlag = (uint8 & 0b00000100) >> 2;
    defaultAssetFlag = (uint8 & 0b00000010) >> 1;
    assetClockRelationFlag = uint8 & 0b00000001;

    if (assetClockRelationFlag) {
        assetClockRelationId = stream.getBe32U();

        uint8_t uint8 = stream.get8U();
        reserved1 = (uint8 & 0b11111110) >> 1;
        assetTimescaleFlag = uint8 & 0b00000001;
        if (assetTimescaleFlag) {
            assetTimescale = stream.getBe32U();
        }
    }

    locationCount = stream.get8U();
    for (int i = 0; i < locationCount; i++) {
        MmtGeneralLocationInfo locationInfo;
        if (!locationInfo.unpack(stream)) {
            return false;
        }
        locations.push_back(std::move(locationInfo));
    }

    assetDescriptorsLength = stream.getBe16U();
    if (assetDescriptorsLength) {
        if (!decriptors.unpack(stream)) {
            return false;
        }

    }
    return true;
}

std::optional<uint16_t> MmtMpTable::Asset::getPacketId() const {
    for (const auto& location : locations) {
        if (location.locationType == 0) {
            return location.packetId;
        }
    }

    return std::nullopt;
}

}
