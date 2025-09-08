#pragma once
#include <cstdint>
#include <vector>
#include "mediaTransportDemuxer.h"
#include "atsc3.h"
#include "route.h"
#include "routeSignaling.h"
#include "mediaStream.h"

namespace atsc3 {

class RouteStream : public MediaStream {
public:
    uint32_t transportSessionId;
    std::string fileName;
    std::vector<uint8_t> initMP4;
    uint32_t srcIpAddr;
    uint32_t dstIpAddr;
    uint16_t dstPort;
    std::string language;
    bool encrypted{ false };
    RouteContentType contentType{ RouteContentType::UNKNOWN };
    bool hasInitToi{ false };
    uint32_t initToi{ 0 };

public:
    StreamType getStreamType() const override {
        if (contentType == RouteContentType::AUDIO) {
            return StreamType::AUDIO;
        }
        else if (contentType == RouteContentType::VIDEO) {
            return StreamType::VIDEO;
        }
        else if (contentType == RouteContentType::SUBTITLE) {
            return StreamType::SUBTITLE;
        }
        return StreamType::UNKNOWN;
    }
};

class RouteDemuxer : public MediaTransportDemuxer {
public:
    RouteDemuxer(Service& service) : MediaTransportDemuxer(service) {}
    virtual bool processPacket(Common::ReadStream& stream);

private:
    struct RouteObject {
        uint32_t transportSessionId{ 0 };
        uint32_t count{ 0 };
        std::vector<uint8_t> buffer;
        bool readyToBuffer{ false };
    };

    bool processSls(const std::unordered_map<std::string, std::string>& files);
    bool processRouteObject(const struct RouteObject& object, uint32_t transportObjectId);
    void updateStreamMap();

    std::unordered_map<uint32_t, struct RouteObject> routeObjects;
    std::unordered_map<uint16_t, RouteStream> mapStream;
    RouteStsid stsid;
    RouteMpd mpd;
};

}