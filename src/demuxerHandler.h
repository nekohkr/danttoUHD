#pragma once
#include "streamPacket.h"
#include "streamInfo.h"

class ServiceManager;
class Service;
class DemuxerHandler {
public:
    virtual void onSlt(const ServiceManager& sm) {}
    virtual void onPmt(const Service& service) {}
    virtual void onStreamData(const Service& service, const StreamInfo& stream, const std::vector<StreamPacket>& chunks, const std::vector<uint8_t>& decryptedMP4, uint64_t baseDtsTimestamp) {}

};