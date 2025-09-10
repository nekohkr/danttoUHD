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
class MpeghDecoder;
class Muxer : public atsc3::DemuxerHandler {
public:
    using OutputCallback = std::function<void(const uint8_t*, size_t, uint64_t)>;
    void setOutputCallback(OutputCallback cb);

private:
    virtual void onSlt(const atsc3::ServiceManager& sm) override;
    virtual void onPmt(const atsc3::Service& service, std::vector<std::reference_wrapper<atsc3::MediaStream>> streams) override;
    virtual void onStreamData(const atsc3::Service& service, const atsc3::MediaStream& stream, const std::vector<StreamPacket>& chunks,
        const std::vector<uint8_t>& decryptedMP4) override;

    std::unordered_map<uint16_t, uint8_t> mapCC;
    OutputCallback outputCallback;
    ts::DuckContext duck;

    std::map<uint32_t, AacEncoder> mapAACEncoder;
    std::map<uint32_t, MpeghDecoder*> mapMpeghDecoder;
    
    bool ready{ false };

};