#include "mmtDemuxer.h"
#include <string>
#include "mmtTable.h"
#include "mmtSignalingMessage.h"
#include <unordered_set>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace atsc3 {

bool MmtDemuxer::processPacket(Common::ReadStream& stream) {
    Mmtp mmtp;
    if (!mmtp.unpack(stream)) {
        return false;
    }

    Common::ReadStream payloadStream(mmtp.payload);
    if (mmtp.type == atsc3::MmtpType::Mpu) {
        MmtMpu mpu;
        if (!mpu.unpack(payloadStream)) {
            return false;
        }

        if (mapStream.find(mmtp.packetId) == mapStream.end()) {
            return true;
        }

        mapStream[mmtp.packetId].currentMpuSequenceNumber = mpu.mpuSequenceNumber;

        auto processMfu = [&]() -> bool {
            atsc3::MmtMpuDataUnit dataUnit;
            if (!dataUnit.unpack(payloadStream, mpu.mpuFragmentType, mpu.timedFlag, mpu.aggregationFlag)) {
                return false;
            }

            if (mpu.fragmentationIndicator == MmtFragmentationIndicator::NotFragmented) {
                processMpu(mmtp.packetId, mpu, dataUnit.payload);
                return true;
            }

            auto assembled = mfuAssembler.addFragment(mmtp.packetId, mpu.mpuSequenceNumber, mpu.fragmentationIndicator, dataUnit.payload);
            if (assembled) {
                processMpu(mmtp.packetId, mpu, *assembled);
            }
            return true;
        };

        if (mpu.aggregationFlag == 0) {
            if (!processMfu()) {
                return false;
            }
        }
        else {
            while (payloadStream.leftBytes() > 0) {
                if (!processMfu()) {
                    return false;
                }
            }
        }
    }
    else if (mmtp.type == MmtpType::SignallingMessage) {
        MmtSignalingMessagePayload signalingMessage;
        if (!signalingMessage.unpack(payloadStream)) {
            return false;
        }

        auto processSignalingMessagePayload = [&]() -> bool {
            atsc3::MmtSignalingMessagePayloadEntry entry;
            if (!entry.unpack(payloadStream, signalingMessage.aggregationFlag, signalingMessage.lengthExtensionFlag)) {
                return false;
            }

            if (signalingMessage.fragmentationIndicator == MmtFragmentationIndicator::NotFragmented) {
                processSignalingMessage(mmtp.packetId, entry.payload);
                return true;
            }

            auto assembled = mfuAssembler.addFragment(mmtp.packetId, 0, signalingMessage.fragmentationIndicator, entry.payload);
            if (assembled) {
                processSignalingMessage(mmtp.packetId, *assembled);
            }
            return true;
        };

        if (signalingMessage.aggregationFlag == 0) {
            processSignalingMessagePayload();
        }
        else {
            while (payloadStream.leftBytes() > 0) {
                processSignalingMessagePayload();
            }
        }
    }

    return true;
}

bool MmtDemuxer::processMpu(uint16_t packetId, const MmtMpu& mpu, const std::vector<uint8_t>& data) {
    if (mapStream.find(packetId) == mapStream.end()) {
        return false;
    }

    auto& stream = mapStream[packetId];
    if (mpu.mpuFragmentType == MmtMpuFragmentType::MovieFragmentMetadata) {
        std::vector<uint8_t> fixed(data);

        if (fixed.size() >= 8) {
            uint32_t newMdatLength = Common::swapEndian32(static_cast<uint32_t>(stream.mfuBuffer.size()) + 8);
            memcpy(fixed.data() + fixed.size() - 8, &newMdatLength, 4);
        }

        stream.movieFragmentMetadataBuffer = fixed;
    }
    else if (mpu.mpuFragmentType == MmtMpuFragmentType::MpuMetadata) {
        stream.mpuMetadataBuffer = data;

        if (stream.movieFragmentMetadataBuffer.size() > 0 &&
            stream.mpuMetadataBuffer.size() > 0 &&
            stream.mfuBuffer.size() > 0) {
            std::vector<uint8_t> buffer;
            buffer.reserve(stream.mpuMetadataBuffer.size() + stream.movieFragmentMetadataBuffer.size() + stream.mfuBuffer.size());
            buffer.insert(buffer.end(), stream.movieFragmentMetadataBuffer.begin(), stream.movieFragmentMetadataBuffer.end());
            buffer.insert(buffer.end(), stream.mfuBuffer.begin(), stream.mfuBuffer.end());

            auto ts = stream.getTimestamp();
            onMediaData(stream, std::move(buffer), stream.mpuMetadataBuffer, ts ? *ts : 0);
            stream.mfuBuffer.clear();
        }
    }
    else if (mpu.mpuFragmentType == MmtMpuFragmentType::Mfu) {
        if (stream.movieFragmentMetadataBuffer.size() > 0 && stream.mpuMetadataBuffer.size() > 0) {
            Common::ReadStream mfuStream(data);
            
            MmtMMTHSample mmth;
            if (!mmth.unpack(mfuStream, true)) {
                return false;
            }
            
            std::vector<uint8_t> payload(mfuStream.leftBytes());
            mfuStream.read(payload.data(), mfuStream.leftBytes());

            stream.mfuBuffer.insert(stream.mfuBuffer.end(), payload.begin(), payload.end());
            stream.mdatLength += static_cast<uint32_t>(stream.mfuBuffer.size());
        }
    }

    return true;
}

bool MmtDemuxer::processSignalingMessage(uint16_t packetId, const std::vector<uint8_t>& data) {
    Common::ReadStream stream(data);
    uint16_t messageId = stream.peekBe16U();

    if (messageId == MmtMessageId::PaMessage) {
        MmtPaMessage paMessage;
        if (!paMessage.unpack(stream)) {
            return false;
        }

        paMessage.length;
    }
    else if (MmtMessageId::isMpiMessage(messageId)) {
        MmtMpiMessage mpiMessage;
        if (!mpiMessage.unpack(stream)) {
            return false;
        }

        MmtMpiTable mpiTable;
        if (!mpiTable.unpack(stream)) {
            return false;
        }
    }
    else if (MmtMessageId::isMptMessage(messageId)) {
        MmtMptMessage mptMessage;
        if (!mptMessage.unpack(stream)) {
            return false;
        }

        MmtMpTable mpTable;
        if (!mpTable.unpack(stream)) {
            return false;
        }

        for (const auto& asset : mpTable.assets) {
            auto packetId = asset.getPacketId();
            if (!packetId) {
                continue;
            }

            auto it = std::find_if(
                mapStream.begin(),
                mapStream.end(),
                [&](const std::pair<const uint16_t, MmtStream>& p) {
                    return p.first == *packetId;
                });
            if (it == mapStream.end()) {
                // Find an available idx
                uint32_t i = 0;
                for (i = 0; i < 255; i++) {
                    if (std::find_if(mapStream.begin(), mapStream.end(),
                        [&](const auto& s) { return s.second.idx == i; }) == mapStream.end()) {
                        break;
                    }
                }
                if (i >= 255) {
                    continue;
                }

                MmtStream stream;
                stream.idx = i;
                stream.packetId = *packetId;
                stream.assetType = asset.assetType;
                mapStream[*packetId] = stream;
            }

            for (const auto& descriptor : asset.decriptors.descriptors) {
                if (descriptor->tag() == MmtMpuTimestampDescriptor::kDescriptorTag) {
                    auto mpuTimestampDescriptor = reinterpret_cast<MmtMpuTimestampDescriptor*>(descriptor.get());
                    for (const auto& entry : mpuTimestampDescriptor->entries) {
                        mapStream[*packetId].mpuTimestamps.push_back(entry);
                    }
                }
            }
        }

        // Notify that streams have been updated
        std::vector<std::reference_wrapper<MediaStream>> temp;
        temp.reserve(mapStream.size());
        for (auto& [id, stream] : mapStream) {
            temp.push_back(stream);
        }

        onStreamTable(temp);
    }
    return false;
}

std::optional<uint64_t> MmtStream::getTimestamp() {
    auto it = std::find_if(mpuTimestamps.begin(), mpuTimestamps.end(),
        [this](const auto& entry) { return entry.mpuSequenceNumber == currentMpuSequenceNumber; });

    if (it == mpuTimestamps.end()) {
        return std::nullopt;
    }

    return (*it).mpuPresentationTime;
}

}
