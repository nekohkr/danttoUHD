#include "muxer.h"
#include <map>
#include <regex>
#include <filesystem>
#include <vector>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include "pesPacket.h"
#include "streamPacket.h"
#include "mpeghDecoder.h"
#include "ac3Encoder.h"
#include "stream.h"

namespace {

void hevcProcess(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, const RouteObject& object) {
    bool first = true;
    size_t pos = 0;
    while (pos + 4 <= input.size()) {
        uint32_t nalUnitSize = (input[pos] << 24) | (input[pos + 1] << 16) | (input[pos + 2] << 8) | input[pos + 3];

        pos += 4;
        if (pos + nalUnitSize > input.size()) {
            break;
        }

        output.insert(output.end(), { 0x00, 0x00, 0x00, 0x01 });
        output.insert(output.end(), input.begin() + pos, input.begin() + pos + nalUnitSize);

        if (first) {
            output.insert(output.end(), object.configNalUnits.begin(), object.configNalUnits.end());
            first = false;
        }
        pos += nalUnitSize;
    }
}

uint64_t convertDTS(uint64_t dts_mp4, uint32_t timescale_mp4, uint32_t timescale_ts = 90000) {
    return static_cast<uint64_t>(static_cast<double>(dts_mp4) / timescale_mp4 * timescale_ts);
}

}

namespace StreamType {

constexpr uint8_t VIDEO_MPEG1 = 0x01;
constexpr uint8_t VIDEO_MPEG2 = 0x02;
constexpr uint8_t AUDIO_MPEG1 = 0x03;
constexpr uint8_t AUDIO_MPEG2 = 0x04;
constexpr uint8_t PRIVATE_SECTION = 0x05;
constexpr uint8_t PRIVATE_DATA = 0x06;
constexpr uint8_t ISO_IEC_13818_6_TYPE_D = 0x0d;
constexpr uint8_t AUDIO_AAC = 0x0f;
constexpr uint8_t AUDIO_AAC_LATM = 0x11;
constexpr uint8_t VIDEO_MPEG4 = 0x10;
constexpr uint8_t METADATA = 0x15;
constexpr uint8_t VIDEO_H264 = 0x1b;
constexpr uint8_t VIDEO_HEVC = 0x24;
constexpr uint8_t VIDEO_AC3 = 0x81;

}

bool Muxer::writePat()
{
    ts::PAT pat(0 % 32, true);
    pat.pmts[0x100] = 0x1F0;

    ts::BinaryTable table;
    pat.serialize(duck, table);

    ts::OneShotPacketizer packetizer(duck, ts::PID_PAT);

    for (size_t i = 0; i < table.sectionCount(); i++) {
        const ts::SectionPtr& section = table.sectionAt(i);
        packetizer.addSection(section);

        ts::TSPacketVector packets;
        packetizer.getPackets(packets);
        for (auto& packet : packets) {
            packet.setCC(mapCC[ts::PID_PAT] & 0xF);
            mapCC[ts::PID_PAT]++;

            outputCallback(packet.b, packet.getHeaderSize() + packet.getPayloadSize());
        }
    }
    return false;
}

void Muxer::setOutputCallback(OutputCallback cb)
{
    outputCallback = std::move(cb);
}

void Muxer::onPmt(const std::unordered_map<uint32_t, RouteObject>& objects)
{
    writePat();
    uint16_t pid = 0x1F0;
    ts::PMT tsPmt(0 % 32, true, 0x100);

    for (const auto& object : objects) {
        if (object.second.contentType == ContentType::VIDEO) {
            ts::PMT::Stream stream(&tsPmt, StreamType::VIDEO_HEVC);
            ts::RegistrationDescriptor descriptor;
            descriptor.format_identifier = 0x48455643;
            stream.descs.add(duck, descriptor);

            tsPmt.streams[0x110 + object.second.transportSessionId] = stream;
        }
        if (object.second.contentType == ContentType::AUDIO) {
            ts::PMT::Stream stream(&tsPmt, StreamType::VIDEO_AC3);
            tsPmt.streams[0x110 + object.second.transportSessionId] = stream;
        }
        if (object.second.contentType == ContentType::SUBTITLE) {
            ts::PMT::Stream stream(&tsPmt, StreamType::ISO_IEC_13818_6_TYPE_D);
            tsPmt.streams[0x110 + object.second.transportSessionId] = stream;
        }
    }

    ts::BinaryTable table;
    tsPmt.serialize(duck, table);

    ts::OneShotPacketizer packetizer(duck, pid);

    for (size_t i = 0; i < table.sectionCount(); i++) {
        const ts::SectionPtr& section = table.sectionAt(i);
        packetizer.addSection(section);

        ts::TSPacketVector packets;
        packetizer.getPackets(packets);
        for (auto& packet : packets) {
            packet.setCC(mapCC[pid] & 0xF);
            ++mapCC[pid];

            outputCallback(packet.b, packet.getHeaderSize() + packet.getPayloadSize());
        }
    }
}

void Muxer::onStreamData(const std::vector<StreamPacket>& packets, const RouteObject& object, const std::vector<uint8_t>& decryptedMP4, uint64_t& baseDts, uint32_t transportObjectId)
{
    if (object.contentType == ContentType::VIDEO) {
        for (const auto& packet : packets) {
            AVRational r = { 1, static_cast<int>(object.timescale) };
            AVRational ts = { 1, 90000 };
            uint64_t dts = av_rescale_q(packet.dts, r, ts);
            uint64_t pts = av_rescale_q(packet.pts, r, ts);

            {
                ts::TSPacket packet;
                packet.init(0x1FFF, mapCC[0x1FFF] & 0xF, 0);
                mapCC[0x1FFF]++;
                packet.setPCR(dts * 300, true);

                outputCallback(packet.b, packet.getHeaderSize() + packet.getPayloadSize());
            }


            std::vector<uint8_t> pesOutput;
            std::vector<uint8_t> processed;

            hevcProcess(packet.data, processed, object);
            
            PESPacket pes;
            pes.setPts(pts);
            pes.setDts(dts);
            pes.setStreamId(STREAM_ID_VIDEO_STREAM_0);
            pes.setPayload(&processed);
            pes.pack(pesOutput);

            size_t payloadLength = pesOutput.size();
            int i = 0;
            while (payloadLength > 0) {
                ts::TSPacket packet;
                packet.init(0x110 + object.transportSessionId, mapCC[0x110 + object.transportSessionId] & 0xF);
                ++mapCC[0x110 + object.transportSessionId];
                if (i == 0) {
                    packet.setPUSI();
                }

                const size_t chunkSize = std::min(payloadLength, static_cast<size_t>(188 - packet.getHeaderSize()));
                packet.setPayloadSize(chunkSize);
                memcpy(packet.b + packet.getHeaderSize(), pesOutput.data() + (pesOutput.size() - payloadLength), chunkSize);
                payloadLength -= chunkSize;

                outputCallback(packet.b, packet.getHeaderSize() + packet.getPayloadSize());

                ++i;
            }

        }

    }
    else if (object.contentType == ContentType::AUDIO) {
        std::vector<uint8_t> wav;
        std::vector<uint8_t> ac3;
        mpeghDecode(decryptedMP4, wav);
        if (wav.size() == 0) {
            return;
        }

        ac3Encode(wav, ac3);

        for (int i2 = 0; i2 < 24; i2++) {
            std::vector<uint8_t> tsBuffer;

            PESPacket pes;
            std::vector<uint8_t> pesOutput;
            AVRational r = { 1, static_cast<int>(object.timescale) };
            AVRational ts = { 1, 90000 };
            uint64_t dts = av_rescale_q((baseDts + i2 * 0x600), r, ts);

            pes.setPts(dts);
            pes.setDts(dts);
            pes.setStreamId(STREAM_ID_AUDIO_STREAM_0);

            std::vector<uint8_t> chunk;
            const size_t chunkSize = ac3.size() / 24;
            chunk.insert(chunk.end(), ac3.begin() + i2 * chunkSize, ac3.begin() + (i2 + 1) * chunkSize);
            pes.setPayload(&chunk);
            pes.pack(pesOutput);

            size_t payloadLength = pesOutput.size();
            int i = 0;

            while (payloadLength > 0) {
                ts::TSPacket packet;
                packet.init(0x110 + object.transportSessionId, mapCC[0x110 + object.transportSessionId] & 0xF);
                ++mapCC[0x110 + object.transportSessionId];

                if (i == 0) {
                    packet.setPUSI();
                }

                const size_t chunkSize = std::min(payloadLength, static_cast<size_t>(188 - packet.getHeaderSize()));
                packet.setPayloadSize(chunkSize);
                memcpy(packet.b + packet.getHeaderSize(), pesOutput.data() + (pesOutput.size() - payloadLength), chunkSize);
                payloadLength -= chunkSize;


                outputCallback(packet.b, packet.getHeaderSize() + packet.getPayloadSize()); 
                ++i;
            }
        }
    }
    else if (object.contentType == ContentType::SUBTITLE) {
    }
    else {
    }

}
