#pragma once
#include <cstdint>
#include "stream.h"

namespace atsc3 {

class RouteLayeredCodingTransport {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint8_t version;
    uint8_t congestionControlFlag;
    uint8_t protocolSpecificIndication;
    bool transportSessionIdentifierFlag;
    bool transportObjectIdentifierFlag;
    bool halfWordFlag;
    uint8_t reserved;
    bool closeSessionFlag;
    bool closeObjectFlag;
    uint8_t headerLength;
    uint8_t codepoint;
    uint32_t congestionControlInformation;
    uint32_t transportSessionId;
    uint32_t transportObjectId;

};

}