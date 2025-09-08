#pragma once
#include <cstdint>
#include "stream.h"
#include <unordered_map>
#include <functional>
#include "mediaStream.h"
#include "atsc3.h"

namespace atsc3 {

class Service;
class MediaTransportDemuxer {
public:
    MediaTransportDemuxer(Service& service) : service(service) {}
    virtual ~MediaTransportDemuxer() = default;
    virtual bool processPacket(Common::ReadStream& stream) { return true; };
    void setOnMediaDataCallback(std::function<bool(MediaStream&, const std::vector<uint8_t>&, const std::vector<uint8_t>&, uint64_t)> callback) { mediaDataCallback = callback; }
    void setOnStreamTableCallback(std::function<bool(std::vector<std::reference_wrapper<MediaStream>>&)> callback) { onStreamTableCallback = callback; }
    void setServiceCategory(Atsc3ServiceCategory serviceCategory) {
        this->serviceCategory = serviceCategory;
    }

protected:
    bool onMediaData(MediaStream& streamId, const std::vector<uint8_t>& mfu, const std::vector<uint8_t>& metadata, uint64_t basePts) {
        if (mediaDataCallback) {
            return mediaDataCallback(streamId, mfu, metadata, basePts);
        }
        return true;
    };
    bool onStreamTable(std::vector<std::reference_wrapper<MediaStream>>& streams) {
        if (onStreamTableCallback) {
            return onStreamTableCallback(streams);
        }
        return true;
    };

    std::function<bool(MediaStream&, const std::vector<uint8_t>&, const std::vector<uint8_t>&, uint64_t)> mediaDataCallback;
    std::function<bool(std::vector<std::reference_wrapper<MediaStream>>&)> onStreamTableCallback;
    Service& service;
    atsc3::Atsc3ServiceCategory serviceCategory;
};

}