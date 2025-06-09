#pragma once
#include <cstdint>
#include <string>
#include "stream.h"
#include "atsc3.h"
#include "lct.h"
#include "routeObject.h"
#include <unordered_map>
#include "demuxerHandler.h"
#include "stsid.h"
#include "mpd.h"
#include "streamInfo.h"

class Service {
public:
    Service(DemuxerHandler** demuxerHandler) : demuxerHandler(demuxerHandler) {
    }

    uint16_t getPmtPid() const {
        return 0x100 + idx * 0x10;
    }

    bool onALP(const ATSC3::LCT& lct, const std::vector<uint8_t>& payload);
    bool processRouteObject(RouteObject& object, uint32_t transportObjectId);
    bool processSLS(const std::unordered_map<std::string, std::string>& files);

    bool processSignalingSTSID(const std::string& xml);
    bool processSignalingMPD(const std::string& xml);
    void updateStreamMap();
    
    std::optional<std::reference_wrapper<StreamInfo>> findStream(uint32_t transportSessionId);


    bool isMediaService() const {
        return serviceCategory == ATSC3::ServiceCategory::LinearAVService ||
            serviceCategory == ATSC3::ServiceCategory::LinearAudioOnlyService;
    }

    struct File {
        uint32_t toi;
        std::string fileName;
    };


    std::unordered_map<uint32_t, struct StreamInfo> mapStream;

    uint32_t idx;
    uint32_t serviceId;
    ATSC3::ServiceCategory serviceCategory;
    std::string shortServiceName;

    uint32_t slsProtocol;
    uint32_t slsMajorProtocolVersion;
    uint32_t slsMinorProtocolVersion;

    uint32_t slsDestinationIpAddress;
    uint16_t slsDestinationUdpPort;
    uint32_t slsSourceIpAddress;

    STSID stsid;
    MPD mpd;

private:
    std::unordered_map<uint32_t, RouteObject> routeObjects;
    DemuxerHandler** demuxerHandler;
};