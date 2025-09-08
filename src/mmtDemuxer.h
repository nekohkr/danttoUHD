#pragma once
#include <cstdint>
#include <functional>
#include "stream.h"
#include "mmt.h"
#include "mmtAssembler.h"
#include <memory>
#include "mediaTransportDemuxer.h"
#include "mediaStream.h"
#include "mmtDescriptor.h"

namespace atsc3 {

 class MmtStream : public MediaStream {
 public:
    virtual StreamType getStreamType() const override {
        if (assetType == MmtAssetType::mp4a) {
            return StreamType::AUDIO;
        }
        else if (assetType == MmtAssetType::mhm1) {
            return StreamType::AUDIO;
        }
        else if (assetType == MmtAssetType::hev1) {
            return StreamType::VIDEO;
        }
        else if (assetType == MmtAssetType::stpp) {
            return StreamType::SUBTITLE;
        }
        return StreamType::UNKNOWN;
    }

    std::optional<uint64_t> getTimestamp();

 public:
     std::vector<uint8_t> mfuBuffer;
     std::vector<uint8_t> movieFragmentMetadataBuffer;
     std::vector<uint8_t> mpuMetadataBuffer;
     std::vector<MmtMpuTimestampDescriptor::Entry> mpuTimestamps;
     uint32_t assetType{0};
     uint32_t mdatLength{0};
     uint32_t mpuSequenceNumber{0};

};

class MmtDemuxer : public MediaTransportDemuxer {
public:
    MmtDemuxer(Service& service) : MediaTransportDemuxer(service) {}
    virtual bool processPacket(Common::ReadStream& stream);

private:
    bool processMpu(uint16_t packetId, const MmtMpu& mpu, const std::vector<uint8_t>& data);
    bool processSignalingMessage(uint16_t packetId, const std::vector<uint8_t>& data);


    std::unordered_map<uint16_t, MmtStream> mapStream;
    MmtAssembler mfuAssembler;
};

}