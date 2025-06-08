#include "demuxer.h"
#include <iomanip>
#include <sstream>
#include <winsock2.h>
#include "stream.h"
#include "ipv4.h"
#include "udp.h"
#include "mp4Processor.h"
#include "pugixml.hpp"
#include "atsc3.h"
#include "bbPacket.h"
#include "alp.h"
#include "lct.h"
#include "lls.h"

namespace {

std::string extractBoundary(const std::string& header) {
    size_t pos = header.find("boundary=");
    if (pos == std::string::npos) return "";
    pos += 9;
    if (header[pos] == '"') {
        size_t end = header.find('"', pos + 1);
        return header.substr(pos + 1, end - pos - 1);
    }
    else {
        size_t end = header.find(';', pos);
        return header.substr(pos, end - pos);
    }
}

std::map<std::string, std::string> parseMultipartRelated(const std::string& data) {
    size_t header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos) return {};
    std::string header = data.substr(0, header_end);
    std::string boundary = extractBoundary(header);
    if (boundary.empty()) return {};

    std::map<std::string, std::string> files;
    std::string delimiter = "--" + boundary;
    size_t start = header_end + 2, end = 0;
    while ((start = data.find(delimiter, start)) != std::string::npos) {
        start += delimiter.size();
        if (data[start] == '-') break;
        start = data.find("\r\n", start) + 1;
        size_t content_header_end = data.find("\r\n\r\n", start);
        std::string headers = data.substr(start, content_header_end - start);

        size_t loc_pos = headers.find("Content-Location:");
        if (loc_pos == std::string::npos) continue;
        size_t loc_start = headers.find_first_not_of(" ", loc_pos + 16);
        size_t loc_end = headers.find("\r\n", loc_start);
        std::string filename = headers.substr(loc_start + 1, loc_end - loc_start);

        size_t type_pos = headers.find("Content-Type:");
        if (type_pos == std::string::npos) continue;
        size_t type_start = headers.find_first_not_of(" ", type_pos + 12);
        size_t type_end = headers.find("\r\n", type_start);
        std::string contentType = headers.substr(type_start + 1, type_end - type_start);

        if (contentType.find("application/dash+xml") != std::string::npos) {
            filename = "mpd.xml";
        }
        if (contentType.find("application/s-tsid") != std::string::npos) {
            filename = "stsid.xml";
        }
        size_t content_start = content_header_end + 2;
        size_t content_end = data.find(delimiter, content_start);
        if (content_end == std::string::npos) continue;
        std::string content = data.substr(content_start, content_end - content_start);
        files[filename] = content;
        start = content_end;
    }
    return files;
}

}

DemuxStatus Demuxer::demux(const std::vector<uint8_t>& input) {
    const auto callback = [this](const LgContainer& lgContainer) {
        Common::ReadStream s(lgContainer.payload);

        ATSC3::BasebandPacket basebandPacket;
        if (!basebandPacket.unpack(s)) {
            return;
        }

        uint32_t alpOffset = 0;
        if (!alpAligned) {
            alpOffset = basebandPacket.baseField.pointer;
            if (basebandPacket.baseField.pointer == 8191) {
                return;
            }
            alpAligned = true;
        }

        Common::ReadStream bbPayloadStream(basebandPacket.payload);
        bbPayloadStream.skip(alpOffset);

        size_t oldSize = alpBuffer.size();
        alpBuffer.resize(alpBuffer.size() + bbPayloadStream.leftBytes());
        bbPayloadStream.read(alpBuffer.data() + oldSize, bbPayloadStream.leftBytes());

        while (alpBuffer.size() > 2) {
            Common::ReadStream alpStream(alpBuffer);
            ATSC3::ALP alp;
            ATSC3::UnpackResult result = alp.unpack(alpStream);

            if (result == ATSC3::UnpackResult::NotEnoughData) {
                return;
            }

            ATSC3::AlpPacketType packetType = static_cast<ATSC3::AlpPacketType>(alp.packetType);
            if (packetType == ATSC3::AlpPacketType::IPv4) {
                Common::ReadStream payloadStream(alp.payload);
                if (!processIpUdp(payloadStream)) {
                    isFirstPacket = true;
                    alpBuffer.clear();
                    return;
                }
            }

            alpBuffer.erase(alpBuffer.begin(), alpBuffer.begin() + (alpBuffer.size() - alpStream.leftBytes()));
        }
    };

    lgContainerUnpacker.addBuffer(input);
    lgContainerUnpacker.unpack(callback);

    return DemuxStatus::Ok;
}

void Demuxer::setHandler(DemuxerHandler* handler)
{
    this->handler = handler;
}

bool Demuxer::processIpUdp(Common::ReadStream& stream)
{
    IPv4Header ipv4;
    if (!ipv4.unpack(stream)) {
        return false;
    }
    this->ipv4 = ipv4;

    UDPHeader udp;
    if (!udp.unpack(stream)) {
        return false;
    }

    size_t leftBytes = stream.leftBytes();
    if (udp.length != leftBytes + 8) {
        return false;
    }

    if (ipv4.dstIpAddr == inet_addr("224.0.23.60") && udp.dstPort == 4937) {
        processLLS(stream);
        return true;
    }

    if ((ipv4.dstIpAddr == inet_addr("239.255.6.1") && udp.dstPort == 50601) ||
        (ipv4.dstIpAddr == inet_addr("239.255.9.24") && udp.dstPort == 5000) ||
        (ipv4.dstIpAddr == inet_addr("239.255.11.1") && udp.dstPort == 51101) ||
        (ipv4.dstIpAddr == inet_addr("239.255.7.24") && udp.dstPort == 5000)) {
        if (!processLCT(stream)) {
            return false;
        }
        return true;
    }

    return true;
}

bool Demuxer::processLLS(Common::ReadStream& stream)
{
    return true;
}

bool Demuxer::processLCT(Common::ReadStream& stream)
{
    ATSC3::LCT lct;
    if (!lct.unpack(stream)) {
        return false;
    }

    uint16_t sbn = stream.getBe16U();
    uint16_t esid = stream.getBe16U();

    if (routeObjects.find(lct.transportSessionId) == routeObjects.end()) {
        RouteObject object;
        object.transportSessionId = lct.transportSessionId;
        routeObjects[lct.transportSessionId] = object;
    }

    RouteObject& object = routeObjects[lct.transportSessionId];

    if (object.readyToBuffer) {
        size_t size = stream.leftBytes();
        std::vector<uint8_t> buffer(size);
        stream.read(buffer.data(), size);
        object.buffer.insert(object.buffer.end(), buffer.begin(), buffer.end());

        bool sbnChanged = false;
        if (object.sbn == 0xFFFF) {
            object.sbn = 0;
        }
        else {
            if (object.sbn != sbn) {
                sbnChanged = true;
                object.sbn = sbn;
            }
        }
    }

    if (lct.closeObjectFlag) {
        if (object.buffer.size()) {
            if (!processRouteObject(object, lct.transportObjectId)) {
                return false;
            }
            object.buffer.clear();
        }

        object.readyToBuffer = true;
    }
    return true;
}

bool Demuxer::processRouteObject(RouteObject& object, uint32_t transportObjectId)
{
    if (object.transportSessionId == 0) {
        std::string data(object.buffer.begin(), object.buffer.end());

        configFiles = parseMultipartRelated(data);
        processConfigs();
        return true;
    }

    if (!object.inited) {
        return true;
    }

    if (object.hasInitToi) {
        if (object.initToi != transportObjectId && !object.initMP4.size()) {
            return true;
        }

        if (object.initToi == transportObjectId) {
            object.initMP4 = object.buffer;

            MP4ConfigParser::MP4Config mp4Config;
            MP4ConfigParser::parse(object.initMP4, mp4Config);
            object.timescale = mp4Config.timescale;
            object.configNalUnits = mp4Config.configNalUnits;
            return true;
        }
    }

    if (object.transportSessionId != 0 && object.initMP4.size() == 0) {
        // waiting for init MP4 data
        return true;
    }

    std::vector<uint8_t> input;
    std::vector<uint8_t> decryptedMP4;
    std::vector<StreamPacket> packets;
    uint64_t baseDts = 0;

    input.insert(input.end(), object.initMP4.begin(), object.initMP4.end());
    input.insert(input.end(), object.buffer.begin(), object.buffer.end());

    if (object.contentType == ContentType::VIDEO ||
        object.contentType == ContentType::AUDIO) {
        bool ret = mp4Processor.process(input, packets, decryptedMP4, baseDts);
    }
    else if (object.contentType == ContentType::SUBTITLE) {
        // todo
    }

    if (handler != nullptr) {
        handler->onStreamData(packets, object, decryptedMP4, baseDts, transportObjectId);
    }

    return true;
}

bool Demuxer::processConfigs()
{
    struct File {
        uint32_t toi;
        std::string fileName;
    };
    struct StreamInfo {
        uint32_t tsi;
        std::vector<File> files;
    };

    std::map<std::string, struct StreamInfo> mapStream;

    std::string stsid = configFiles["stsid.xml"];
    if (stsid == "") {
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(stsid.data(), stsid.size());
    for (pugi::xml_node& p : doc.child("S-TSID").child("RS").children("LS")) {
        std::string tsi, fileName;
        if (!p.attribute("tsi")) {
            continue;
        }

        struct StreamInfo info;
        tsi = p.attribute("tsi").value();
        fileName = p.child("SrcFlow").child("EFDT").child("FileTemplate").child_value();
        size_t pos = fileName.find("$TOI$");
        if (pos != std::string::npos) {
            fileName.replace(pos, 5, "1234");
        }
        for (pugi::xml_node& file : p.child("SrcFlow").child("EFDT").child("FDTParameters").children("File")) {
            if (!file.attribute("TOI")) {
                continue;
            }
            if (!file.attribute("Content-Location")) {
                continue;
            }
            File fileInfo;
            fileInfo.toi = std::stoul(file.attribute("TOI").value());
            fileInfo.fileName = file.attribute("Content-Location").value();
            info.files.push_back(fileInfo);
        }
        info.tsi = std::stoul(tsi);

        mapStream[fileName] = info;
    }


    std::string mpd = configFiles["mpd.xml"];
    if (mpd == "") {
        return false;
    }

    result = doc.load_buffer(mpd.data(), mpd.size());
    for (pugi::xml_node& p : doc.child("MPD").child("Period").children("AdaptationSet")) {
        std::string contentType;
        std::string id;
        bool encrypted = false;
        if (!p.child("Representation").attribute("id")) {
            continue;
        }

        if (p.child("ContentProtection")) {
            if (strcmp(p.child("ContentProtection").attribute("value").value(), "Digicap") == 0 ||
                strcmp(p.child("ContentProtection").attribute("value").value(), "cenc") == 0) {
                encrypted = true;
            }
        }
        id = p.child("Representation").attribute("id").value();

        std::string initFileName, fileName;
        if (p.child("Representation").child("SegmentTemplate").attribute("initialization")) {
            initFileName = p.child("Representation").child("SegmentTemplate").attribute("initialization").value();
        }
        if (!p.child("Representation").child("SegmentTemplate").attribute("media")) {
            continue;
        }
        fileName = p.child("Representation").child("SegmentTemplate").attribute("media").value();
        size_t pos = fileName.find("$RepresentationID$");
        if (pos != std::string::npos) {
            fileName.replace(pos, 18, id);
        }
        pos = fileName.find("$Number$");
        if (pos != std::string::npos) {
            fileName.replace(pos, 8, "1234");
        }
        pos = initFileName.find("$RepresentationID$");
        if (pos != std::string::npos) {
            initFileName.replace(pos, 18, id);
        }

        if (mapStream.find(fileName) == mapStream.end()) {
            continue;
        }

        uint32_t tsi = mapStream[fileName].tsi;
        if (p.attribute("contentType")) {
            contentType = p.attribute("contentType").value();

            if (contentType == "video") {
                routeObjects[tsi].contentType = ContentType::VIDEO;
            }
            else if (contentType == "audio") {
                routeObjects[tsi].contentType = ContentType::AUDIO;
            }
            else if (contentType == "text") {
                routeObjects[tsi].contentType = ContentType::SUBTITLE;
            }
            else {
                routeObjects[tsi].contentType = ContentType::UNKNOWN;
            }
        }
        else {
            if (p.attribute("mimeType")) {
                contentType = p.attribute("mimeType").value();
                if (contentType == "video/mp4") {
                    routeObjects[tsi].contentType = ContentType::VIDEO;
                }
                else if (contentType == "audio/mp4") {
                    routeObjects[tsi].contentType = ContentType::AUDIO;
                }
                else if (contentType == "application/mp4") {
                    routeObjects[tsi].contentType = ContentType::SUBTITLE;
                }
                else {
                    routeObjects[tsi].contentType = ContentType::UNKNOWN;
                }
            }
            else {
                contentType = p.child("Representation").attribute("mimeType").value();
                if (contentType == "video/mp4") {
                    routeObjects[tsi].contentType = ContentType::VIDEO;
                }
                else if (contentType == "audio/mp4") {
                    routeObjects[tsi].contentType = ContentType::AUDIO;
                }
                else if (contentType == "application/mp4") {
                    routeObjects[tsi].contentType = ContentType::SUBTITLE;
                }
                else {
                    routeObjects[tsi].contentType = ContentType::UNKNOWN;
                }
            }
        }


        if (initFileName != "") {
            for (const auto& file : mapStream[fileName].files) {
                if (file.fileName == initFileName) {
                    routeObjects[tsi].hasInitToi = true;
                    routeObjects[tsi].initToi = file.toi;
                }
            }
        }
        routeObjects[tsi].encrypted = encrypted;
        routeObjects[tsi].inited = true;
    }

    if (handler != nullptr) {
        handler->onPmt(routeObjects);
    }
    return true;
}
