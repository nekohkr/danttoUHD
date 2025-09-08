#pragma once
#include <cstdint>
#include <string>
#include <list>
#include "atsc3.h"

namespace atsc3 {

class Atsc3Table {
public:
    virtual ~Atsc3Table() = default;
    virtual uint8_t tableId() const = 0;
    virtual bool unpack(const std::string& xml) = 0;
};

template<uint8_t TID>
class Atsc3TableTemplate : public Atsc3Table {
public:
    static constexpr uint8_t kTableId = TID;
    uint8_t tableId() const override { return TID; }
};

class Atsc3ServiceListTable : public Atsc3TableTemplate<0x01> {
public:
    bool unpack(const std::string& xml) override;

public:
    struct Service {
        uint32_t serviceId;
        std::string serviceName;
        Atsc3ServiceCategory serviceCategory;
        std::string shortServiceName;
        uint32_t slsProtocol;
        uint32_t slsMajorProtocolVersion;
        uint32_t slsMinorProtocolVersion;
        uint32_t slsDestinationIpAddress;
        uint16_t slsDestinationUdpPort;
        uint32_t slsSourceIpAddress;

    };

    uint32_t bsid{ 0 };
    std::list<struct Service> services;
};

}