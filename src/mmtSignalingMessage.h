#pragma once
#include <cstdint>
#include "stream.h"
#include <vector>
#include <list>

namespace atsc3 {

class MmtSignalingMessage {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint16_t messageId;
    uint8_t version;

};

class MmtPaMessage : public MmtSignalingMessage {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint32_t length;
    uint8_t tableCount;
    struct TableInfo {
        uint8_t id;
        uint8_t version;
        uint16_t length;
    };

    std::list<struct TableInfo> tableInfos;

};

class MmtMpiMessage : public MmtSignalingMessage {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint32_t length;
    bool associatedMpTableFlag;

};

class MmtMptMessage : public MmtSignalingMessage {
public:
    bool unpack(Common::ReadStream& stream);

public:
    uint16_t length;

};

}