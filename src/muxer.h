#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <functional>
#include <tsduck.h>
#include "streamPacket.h"
#include "demuxerHandler.h"
#include "serviceManager.h"
#include "aacEncoder.h"

struct AVCodecContext;

class Muxer : public DemuxerHandler {
public:
    using OutputCallback = std::function<void(const uint8_t*, size_t, uint64_t)>;
    void setOutputCallback(OutputCallback cb);

private:
    virtual void onSlt(const ServiceManager& sm) override;
    virtual void onPmt(const Service& service) override;
    virtual void onStreamData(const Service& service, const StreamInfo& stream, const std::vector<StreamPacket>& chunks, const std::vector<uint8_t>& decryptedMP4, int64_t baseDtsTimestamp) override;

    std::unordered_map<uint16_t, uint8_t> mapCC;
    OutputCallback outputCallback;
    ts::DuckContext duck;

    std::map<uint32_t, AacEncoder> mapAACEncoder;
    bool ready{ false };

};