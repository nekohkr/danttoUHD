#pragma once
#include <unordered_map>
#include <string>
#include "ipv4.h"
#include "mp4Processor.h"
#include "demuxerHandler.h"
#include "lgContainer.h"
#include "stream.h"
#include "serviceManager.h"
#include "pcapWriter.h"
#include "mmtDemuxer.h"
#include "atsc3Table.h"

namespace atsc3 {

enum class DemuxStatus {
    Ok = 0x0000,
    NotEnoughBuffer = 0x1000,
    NotValidBBFrame = 0x1001,
    WattingForEcm = 0x1002,
    Error = 0x2000,
};

class Demuxer {
public:
    DemuxStatus demux(const std::vector<uint8_t>& input);
    void setHandler(DemuxerHandler* handler);

private:
    bool processIpUdp(Common::ReadStream& stream);
    bool processLls(Common::ReadStream& stream);
    bool processSlt(const atsc3::Atsc3ServiceListTable& slt);
    
    std::unordered_map<uint32_t, uint8_t> mapCC;
    std::vector<uint8_t> alpBuffer;
    bool alpAligned{ false };
    MP4Processor mp4Processor;
    DemuxerHandler* handler;
    ServiceManager serviceManager;
    LgContainerUnpacker lgContainerUnpacker;
    PcapWriter pcapWriter;
    MP4Processor mp4processor;
};

}