#pragma once
#include "streamPacket.h"
#include "mediaTransportDemuxer.h"

namespace atsc3 {

class ServiceManager;
class Service;
class DemuxerHandler {
public:
    virtual void onSlt(const ServiceManager& sm) {}
    virtual void onPmt(const Service& service, std::vector<std::reference_wrapper<atsc3::MediaStream>> streams) {}
    virtual void onStreamData(const Service& service, const atsc3::MediaStream& stream, const std::vector<StreamPacket>& chunks) {}

};

}