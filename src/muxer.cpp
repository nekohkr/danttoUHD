#include "muxer.h"
#include <map>
#include <regex>
#include <filesystem>
#include <vector>
#include "pesPacket.h"
#include "streamPacket.h"
#include "mpeghDecoder.h"
#include "stream.h"
#include "service.h"
#include "rescale.h"
#include "mp4Processor.h"
#include <Ap4HevcParser.h>

namespace {

void hevcProcess(const MP4ConfigParser::MP4Config& mp4Config, const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    bool have_access_unit_delimiter = false;
    bool have_param_sets = false;

    Common::ReadStream s(input);
    while (s.leftBytes()) {
        if (s.leftBytes() < mp4Config.nalUnitLengthSize) {
            return;
        }
        uint32_t nalUnitSize = 0;
        if (mp4Config.nalUnitLengthSize == 1) {
            nalUnitSize = s.get8U();
        }
        else if (mp4Config.nalUnitLengthSize == 2) {
            nalUnitSize = s.getBe16U();
        }
        else if (mp4Config.nalUnitLengthSize == 3) {
            nalUnitSize = s.getBe16U() << 8;
            nalUnitSize |= s.get8U();
        }
        else if (mp4Config.nalUnitLengthSize == 4) {
            nalUnitSize = s.getBe32U();
        }
        else {
            return;
        }

        if (s.leftBytes() < nalUnitSize) {
            return;
        }

        unsigned int nal_unit_type = (s.peek8U() >> 1) & 0x3F;
        if (nal_unit_type == AP4_HEVC_NALU_TYPE_AUD_NUT) {
            have_access_unit_delimiter = true;
        }
        if (nal_unit_type == AP4_HEVC_NALU_TYPE_VPS_NUT ||
            nal_unit_type == AP4_HEVC_NALU_TYPE_SPS_NUT ||
            nal_unit_type == AP4_HEVC_NALU_TYPE_PPS_NUT) {
            have_param_sets = true;
            break;
        }

        s.skip(nalUnitSize);
    }

    if (!have_access_unit_delimiter) {
        output.insert(output.end(), {0, 0, 0, 1, AP4_HEVC_NALU_TYPE_AUD_NUT << 1 , 1, 0x40});
    }

    bool prefix_added = false;
    s.seek(0);
    while (s.leftBytes()) {
        uint32_t nalUnitSize = 0;
        if (mp4Config.nalUnitLengthSize == 1) {
            nalUnitSize = s.get8U();
        }
        else if (mp4Config.nalUnitLengthSize == 2) {
            nalUnitSize = s.getBe16U();
        }
        else if (mp4Config.nalUnitLengthSize == 3) {
            nalUnitSize = s.getBe16U() << 8;
            nalUnitSize |= s.get8U();
        }
        else if (mp4Config.nalUnitLengthSize == 4) {
            nalUnitSize = s.getBe32U();
        }
        else {
            return;
        }

        if (s.leftBytes() < nalUnitSize) {
            return;
        }

        if (!prefix_added) {
            output.insert(output.end(), mp4Config.prefixNalUnits.begin(), mp4Config.prefixNalUnits.end());
            prefix_added = true;
        }

        std::vector<uint8_t> buffer;
        buffer.resize(nalUnitSize);
        s.read(buffer.data(), nalUnitSize);

        output.insert(output.end(), { 0, 0, 1 });
        output.insert(output.end(), buffer.begin(), buffer.end());
    }
}

uint64_t convertDTS(uint64_t dts_mp4, uint32_t timescale_mp4, uint32_t timescale_ts = 90000) {
    return static_cast<uint64_t>(static_cast<double>(dts_mp4) / timescale_mp4 * timescale_ts);
}

uint16_t calcPesPid(const Service& service, uint32_t streamIdx) {
    return service.getPmtPid() + 1 + streamIdx;
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

void Muxer::setOutputCallback(OutputCallback cb)
{
    outputCallback = std::move(cb);
}

void Muxer::onSlt(const ServiceManager& sm)
{
    if (!ready) {
        return;
    }

    {
        std::vector<uint8_t> tsBuffer;
        ts::PAT pat(0, true, sm.bsid, sm.bsid);
        for (const auto& service : sm.services) {
            if (!service.isMediaService()) {
                continue;
            }
            pat.pmts[service.serviceId] = service.getPmtPid();
        }

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

                tsBuffer.insert(tsBuffer.end(), packet.b, packet.b + packet.getHeaderSize() + packet.getPayloadSize());
            }
        }
        outputCallback(tsBuffer.data(), tsBuffer.size(), 0);
    }

    {
        std::vector<uint8_t> tsBuffer;
        ts::SDT sdt(true, 0, true, sm.bsid, sm.bsid);
        for (const auto& service : sm.services) {
            if (!service.isMediaService()) {
                continue;
            }

            ts::SDT::ServiceEntry tsService(&sdt);
            ts::ServiceDescriptor tsDescriptor;
            tsDescriptor.service_name = ts::UString::FromUTF8(service.shortServiceName);
            tsDescriptor.service_type = 1;
            tsService.descs.add(duck, tsDescriptor);
            sdt.services[service.serviceId] = tsService;
        }

        ts::BinaryTable table;
        sdt.serialize(duck, table);

        ts::OneShotPacketizer packetizer(duck, ts::PID_SDT);

        for (size_t i = 0; i < table.sectionCount(); i++) {
            const ts::SectionPtr& section = table.sectionAt(i);
            packetizer.addSection(section);

            ts::TSPacketVector packets;
            packetizer.getPackets(packets);
            for (auto& packet : packets) {
                packet.setCC(mapCC[ts::PID_SDT] & 0xF);
                mapCC[ts::PID_SDT]++;

                tsBuffer.insert(tsBuffer.end(), packet.b, packet.b + packet.getHeaderSize() + packet.getPayloadSize());
            }
        }
        outputCallback(tsBuffer.data(), tsBuffer.size(), 0);
    }

    {
        std::vector<uint8_t> tsBuffer;
        ts::NIT nit(true, 0, true, sm.bsid);
        ts::NetworkNameDescriptor tsDescriptor;
        tsDescriptor.name = ts::UString::FromUTF8("danttoUHD (https://github.com/nekohkr/danttoUHD)");
        nit.descs.add(duck, tsDescriptor);

        ts::BinaryTable table;
        nit.serialize(duck, table);

        ts::OneShotPacketizer packetizer(duck, ts::PID_NIT);

        for (size_t i = 0; i < table.sectionCount(); i++) {
            const ts::SectionPtr& section = table.sectionAt(i);
            packetizer.addSection(section);

            ts::TSPacketVector packets;
            packetizer.getPackets(packets);
            for (auto& packet : packets) {
                packet.setCC(mapCC[ts::PID_NIT] & 0xF);
                mapCC[ts::PID_NIT]++;

                tsBuffer.insert(tsBuffer.end(), packet.b, packet.b + packet.getHeaderSize() + packet.getPayloadSize());
            }
        }
        outputCallback(tsBuffer.data(), tsBuffer.size(), 0);
    }

}

void Muxer::onPmt(const Service& service)
{
    if (!ready) {
        return;
    }

    if (!service.isMediaService()) {
        return;
    }

    uint16_t pcrPid = 0x1fff;
    for (const auto& stream : service.mapStream) {
        if (stream.second.contentType == ContentType::VIDEO) {
            pcrPid = calcPesPid(service, stream.second.idx);
            break;
        }
    }

    uint16_t pid = service.getPmtPid();
    ts::PMT tsPmt(0, true, pcrPid);
    tsPmt.service_id = service.serviceId;

    for (const auto& stream : service.mapStream) {
        if (stream.second.contentType == ContentType::VIDEO) {
            ts::PMT::Stream tsStream(&tsPmt, StreamType::VIDEO_HEVC);
            ts::RegistrationDescriptor descriptor;
            descriptor.format_identifier = 0x48455643;
            tsStream.descs.add(duck, descriptor);

            tsPmt.streams[calcPesPid(service, stream.second.idx)] = tsStream;
        }
        if (stream.second.contentType == ContentType::AUDIO) {
            ts::PMT::Stream tsStream(&tsPmt, StreamType::AUDIO_AAC);
            tsPmt.streams[calcPesPid(service, stream.second.idx)] = tsStream;
        }
        if (stream.second.contentType == ContentType::SUBTITLE) {
            ts::PMT::Stream tsStream(&tsPmt, StreamType::ISO_IEC_13818_6_TYPE_D);
            tsPmt.streams[calcPesPid(service, stream.second.idx)] = tsStream;
        }
    }

    ts::BinaryTable table;
    tsPmt.serialize(duck, table);

    ts::OneShotPacketizer packetizer(duck, pid);

    std::vector<uint8_t> tsBuffer;
    for (size_t i = 0; i < table.sectionCount(); i++) {
        const ts::SectionPtr& section = table.sectionAt(i);
        packetizer.addSection(section);

        ts::TSPacketVector packets;
        packetizer.getPackets(packets);
        for (auto& packet : packets) {
            packet.setCC(mapCC[pid] & 0xF);
            ++mapCC[pid];

            tsBuffer.insert(tsBuffer.end(), packet.b, packet.b + packet.getHeaderSize() + packet.getPayloadSize());
        }
    }

    outputCallback(tsBuffer.data(), tsBuffer.size(), 0);
}

void Muxer::onStreamData(const Service& service, const StreamInfo& stream, const std::vector<StreamPacket>& packets, const std::vector<uint8_t>& decryptedMP4, int64_t baseDtsTimestamp)
{
    std::vector<uint8_t> tsBuffer;
    uint16_t pid = calcPesPid(service, stream.idx);
    AVRational r = { 1, static_cast<int>(stream.mp4Config.timescale) };
    AVRational ts = { 1, 90000 };
    uint64_t baseDts = av_rescale_q(packets[0].dts, r, ts);

    if (stream.contentType == ContentType::VIDEO) {
        for (const auto& packet : packets) {
            uint64_t dts = av_rescale_q(packet.dts, r, ts);
            uint64_t pts = av_rescale_q(packet.pts, r, ts);
            uint64_t dtsTimestamp = baseDtsTimestamp + (dts - baseDts) / 90;

            std::vector<uint8_t> pesOutput;
            std::vector<uint8_t> processed;

            hevcProcess(stream.mp4Config, packet.data, processed);

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
                packet.init(pid, mapCC[pid] & 0xF);
                ++mapCC[pid];
                if (i == 0) {
                    packet.setPUSI();
                    packet.setPCR(dts * 300, true);
                }

                const size_t chunkSize = std::min(payloadLength, static_cast<size_t>(188 - packet.getHeaderSize()));
                packet.setPayloadSize(chunkSize);
                memcpy(packet.b + packet.getHeaderSize(), pesOutput.data() + (pesOutput.size() - payloadLength), chunkSize);
                payloadLength -= chunkSize;

                tsBuffer.insert(tsBuffer.end(), packet.b, packet.b + packet.getHeaderSize() + packet.getPayloadSize());
                ++i;
            }

            outputCallback(tsBuffer.data(), tsBuffer.size(), dtsTimestamp);
            tsBuffer.clear();
            ready = true;
        }
    }
    else if (stream.contentType == ContentType::AUDIO) {
        std::vector<uint8_t> wav;
        std::vector<std::vector<uint8_t>> aac;
        mpeghDecode(decryptedMP4, wav);
        if (wav.size() == 0) {
            return;
        }

        uint32_t streamKey = service.idx << 16 | stream.idx;
        mapAACEncoder[streamKey].encode(wav, aac);
        if (aac.size() == 0) {
            return;
        }

        uint64_t duration = (packets[1].dts - packets[0].dts) * packets.size();
        uint64_t durationPerFrame = duration / aac.size();

        int j = 0;
        for (const auto& item : aac) {
            PESPacket pes;
            std::vector<uint8_t> pesOutput;
            AVRational r = { 1, static_cast<int>(stream.mp4Config.timescale) };
            AVRational ts = { 1, 90000 };
            uint64_t dts = av_rescale_q(packets[0].dts + j * durationPerFrame, r, ts);
            uint64_t dtsTimestamp = baseDtsTimestamp + (dts - baseDts) / 90;
            ++j;

            pes.setPts(dts);
            pes.setDts(dts);
            pes.setStreamId(STREAM_ID_AUDIO_STREAM_0);
            pes.setPayload(&item);
            pes.pack(pesOutput);

            size_t payloadLength = pesOutput.size();
            int i = 0;

            while (payloadLength > 0) {
                ts::TSPacket packet;
                packet.init(pid, mapCC[pid] & 0xF);
                ++mapCC[pid];

                if (i == 0) {
                    packet.setPUSI();
                }

                const size_t chunkSize = std::min(payloadLength, static_cast<size_t>(188 - packet.getHeaderSize()));
                packet.setPayloadSize(chunkSize);
                memcpy(packet.b + packet.getHeaderSize(), pesOutput.data() + (pesOutput.size() - payloadLength), chunkSize);
                payloadLength -= chunkSize;

                tsBuffer.insert(tsBuffer.end(), packet.b, packet.b + packet.getHeaderSize() + packet.getPayloadSize());
                ++i;
            }

            outputCallback(tsBuffer.data(), tsBuffer.size(), dtsTimestamp);
            tsBuffer.clear();
        }
    }
    else if (stream.contentType == ContentType::SUBTITLE) {

    }

}
