#pragma once
#include <cstdint>
#include "stream.h"
#include <list>
#include <optional>
#include "mmt.h"
#include "mmtDescriptor.h"

namespace atsc3 {

class MmtTable {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint8_t tableId;
    uint8_t version;
    uint16_t length;

};

class MmtMpiTable : public MmtTable {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint8_t reserved0;
    uint8_t piMode;
    uint8_t reserved1;
    uint16_t mpitDescriptorsLength;

};

class MmtpMptTable : public MmtTable {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint8_t reserved0;
    uint8_t piMode;
    uint8_t reserved1;
    uint16_t mpitDescriptorsLength;
    // TODO
};

class MmtMpTable : public MmtTable {
public:
    bool unpack(Common::ReadStream& stream);

public:
    struct AssetId {
        uint32_t assetIdScheme;
        uint32_t assetIdLength;
        std::vector<uint8_t> assetId;
    };

    enum class IdentifierType : uint8_t {
        AssetId = 0x00,
        Urls = 0x01,
        Regex = 0x02,
        RespresentationId = 0x03,
    };

    class IdentifierMapping {
    public:
        bool unpack(Common::ReadStream& stream);

    public:
        IdentifierType identifierType;
        struct AssetId assetId;
        uint16_t urlCount;
        std::list<std::string> urls;
        uint16_t regexLength;
        std::string regex;
        uint16_t representationIdLength;
        std::vector<uint8_t> representationId;
        uint16_t privateLength;
        std::vector<uint8_t> privateByte;

    };


    class Asset {
    public:
        bool unpack(Common::ReadStream& stream);
        std::optional<uint16_t> getPacketId() const;

    public:
        IdentifierMapping identifierMapping;
        uint32_t assetType;
        uint8_t reserved0;
        bool assetModificationFlag;
        bool defaultAssetFlag;
        bool assetClockRelationFlag;

        uint8_t assetClockRelationId;
        uint8_t reserved1;
        bool assetTimescaleFlag;
        uint32_t assetTimescale;

        uint8_t assetCount;

        uint8_t locationCount;
        std::list<MmtGeneralLocationInfo> locations;

        uint16_t assetDescriptorsLength;
        MmtDescriptors decriptors;
    };

    uint8_t reserved0;
    uint8_t mpTableMode;
    uint8_t mmtPackageIdLength;
    std::vector<uint8_t> mmtPackageId;
    uint16_t mpTableDescriptorsLength;
    uint8_t assetCount;
    std::list<Asset> assets;
};

}