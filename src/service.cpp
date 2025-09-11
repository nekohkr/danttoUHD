#include "service.h"
#include <map>
#include <chrono>
#include "pugixml.hpp"
#include "mp4Processor.h"
#include "rescale.h"
#include "mediaTransportDemuxer.h"
#include "demuxer.h"

namespace atsc3 {

Service::Service(DemuxerHandler* handler) : handler(handler), mmtDemuxer(*this), routeDemuxer(*this) {
    auto streamTableCallback = [this](std::vector<std::reference_wrapper<MediaStream>>& streams) {
        this->onStreamTable(streams);
        return true;
    };

    routeDemuxer.setOnStreamTableCallback(streamTableCallback);
    mmtDemuxer.setOnStreamTableCallback(streamTableCallback);


    auto streamDataCallback = [this](MediaStream& stream, const std::vector<uint8_t>& mfu, const std::vector<uint8_t>& metadata, uint64_t basePts) {
        this->onMediaData(stream, mfu, metadata, basePts);
        return true;
     };

    routeDemuxer.setOnMediaDataCallback(streamDataCallback);
    mmtDemuxer.setOnMediaDataCallback(streamDataCallback);
}

bool Service::processPacket(Common::ReadStream& stream) {
    routeDemuxer.setServiceCategory(serviceCategory);
    mmtDemuxer.setServiceCategory(serviceCategory);

    if (slsProtocol == 1) {
        return routeDemuxer.processPacket(stream);
    }
    else if (slsProtocol == 2) {
        return mmtDemuxer.processPacket(stream);
    }
    return true;
}

std::optional<std::reference_wrapper<MediaStream>> Service::findStream(uint32_t transportSessionId) {
    auto it = mapStream.find(transportSessionId);
    if (it != mapStream.end()) {
        return it->second;
    }
    return {};
}


bool Service::onStreamTable(const std::vector<std::reference_wrapper<MediaStream>>& streams) {
    if (handler) {
        handler->onPmt(*this, streams);
    }
    return false;
}

bool Service::onMediaData(atsc3::MediaStream& stream, const std::vector<uint8_t>& mfu, const std::vector<uint8_t>& metadata, uint64_t basePts) {
    std::vector<StreamPacket> packets;

    MP4ConfigParser::parse(metadata, stream.mp4CodecConfig);

    std::vector<uint8_t> input;
    input.insert(input.end(), metadata.begin(), metadata.end());
    input.insert(input.end(), mfu.begin(), mfu.end());

    mp4Processor.process(input, packets);

    if (packets.size() == 0) {
        return false;
    }

    if (basePts != 0) {
        int64_t rescaled = av_rescale(basePts, stream.mp4CodecConfig.timescale, 1000000ll * 1);

        for (auto& packet : packets) {
            packet.dts += rescaled;
            packet.pts += rescaled;
        }
    }

    if (handler != nullptr) {
        handler->onStreamData(*this, stream, packets);
    }

    return true;
}


}