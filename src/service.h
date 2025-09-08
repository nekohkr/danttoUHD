#pragma once
#include <cstdint>
#include <string>
#include "stream.h"
#include "atsc3.h"
#include <unordered_map>
#include "demuxerHandler.h"
#include "mp4Processor.h"
#include "mmtDemuxer.h"
#include "routeDemuxer.h"
#include "routeSignaling.h"
#include "mediaStream.h"

namespace atsc3 {

class Demuxer;
class Service {
public:
    Service(DemuxerHandler* handler);

    uint16_t getPmtPid() const {
        return 0x100 + idx * 0x10;
    }

    bool processPacket(Common::ReadStream& stream);
    std::optional<std::reference_wrapper<MediaStream>> findStream(uint32_t transportSessionId);


    bool isMediaService() const {
        return serviceCategory == atsc3::Atsc3ServiceCategory::LinearAVService ||
            serviceCategory == atsc3::Atsc3ServiceCategory::LinearAudioOnlyService;
    }

    struct File {
        uint32_t toi;
        std::string fileName;
    };

    std::unordered_map<uint32_t, MediaStream> mapStream;

    uint32_t idx;
    uint32_t serviceId;
    atsc3::Atsc3ServiceCategory serviceCategory;
    std::string shortServiceName;

    uint32_t slsProtocol;
    uint32_t slsMajorProtocolVersion;
    uint32_t slsMinorProtocolVersion;

    uint32_t slsDestinationIpAddress;
    uint16_t slsDestinationUdpPort;
    uint32_t slsSourceIpAddress;


private:
    bool onStreamTable(const std::vector<std::reference_wrapper<MediaStream>>& streams);
    bool onMediaData(atsc3::MediaStream& stream, const std::vector<uint8_t>& mfu, const std::vector<uint8_t>& metadata, uint64_t basePts);

    DemuxerHandler* handler{ nullptr };
    MP4Processor mp4Processor;
    atsc3::MmtDemuxer mmtDemuxer;
    atsc3::RouteDemuxer routeDemuxer;

};

}