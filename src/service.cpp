#include "service.h"
#include <map>
#include "pugixml.hpp"
#include "mp4Processor.h"
#include "stsid.h"

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

std::unordered_map<std::string, std::string> parseMultipartRelated(const std::string& data) {
    size_t header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos) return {};
    std::string header = data.substr(0, header_end);
    std::string boundary = extractBoundary(header);
    if (boundary.empty()) return {};

    std::unordered_map<std::string, std::string> files;
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

bool Service::onALP(const ATSC3::LCT& lct, const std::vector<uint8_t>& payload)
{
    if (routeObjects.find(lct.transportSessionId) == routeObjects.end()) {
        RouteObject object;
        object.transportSessionId = lct.transportSessionId;
        routeObjects[lct.transportSessionId] = object;
    }

    RouteObject& object = routeObjects[lct.transportSessionId];

    if (object.readyToBuffer) {
        object.buffer.insert(object.buffer.end(), payload.begin(), payload.end());
    }

    if (lct.closeObjectFlag) {
        if (object.buffer.size()) {
            processRouteObject(object, lct.transportObjectId);
            object.buffer.clear();
        }

        object.readyToBuffer = true;
    }
    return false;
}


bool Service::processRouteObject(RouteObject& object, uint32_t transportObjectId)
{
    if (object.transportSessionId == 0) {
        // SLS
        std::string data(object.buffer.begin(), object.buffer.end());
        auto sls = parseMultipartRelated(data);
        processSLS(sls);
        return true;
    }

    auto stream = findStream(object.transportSessionId);
    if (!stream) {
        return false;
    }

    if (stream->get().hasInitToi) {
        if (stream->get().initToi != transportObjectId && !stream->get().initMP4.size()) {
            // waiting for init MP4 data
            return true;
        }

        if (stream->get().initToi == transportObjectId) {
            stream->get().initMP4 = object.buffer;

            // 굳이 분리해야하나?
            MP4ConfigParser::MP4Config mp4Config;
            MP4ConfigParser::parse(stream->get().initMP4, mp4Config);
            stream->get().timescale = mp4Config.timescale;
            stream->get().configNalUnits = mp4Config.configNalUnits;
            return true;
        }
    }

    std::vector<uint8_t> input;
    std::vector<uint8_t> decryptedMP4;
    std::vector<StreamPacket> packets;
    uint64_t baseDts = 0;

    input.insert(input.end(), stream->get().initMP4.begin(), stream->get().initMP4.end());
    input.insert(input.end(), object.buffer.begin(), object.buffer.end());

    if (stream->get().contentType == ContentType::VIDEO ||
        stream->get().contentType == ContentType::AUDIO) {
        MP4Processor mp4Processor;
        bool ret = mp4Processor.process(input, packets, decryptedMP4, baseDts);
    }
    else if (stream->get().contentType == ContentType::SUBTITLE) {
        // todo
    }

    if (demuxerHandler != nullptr && *demuxerHandler != nullptr) {
        (*demuxerHandler)->onStreamData(*this, stream->get(), packets, decryptedMP4, baseDts, transportObjectId);
    }
    return true;
}

bool Service::processSLS(const std::unordered_map<std::string, std::string>& files)
{
    if (isMediaService()) {
        for (const auto& file : files) {
            if (file.first == "stsid.xml") {
                stsid.parse(file.second);
            }
            else if (file.first == "mpd.xml") {
                mpd.parse(file.second);
            }
        }

        updateStreamMap();
    }

    return true;
}

void Service::updateStreamMap()
{
    std::list<uint32_t> tsiList;

    for (const auto& rs : stsid.rsList) {
        for (const auto& ls : rs.lsList) {
            struct StreamInfo streamInfo;
            streamInfo.transportSessionId = ls.transportSessionId;
            streamInfo.fileName = ls.enhancedFileDeliveryTable.fileTemplate;

            streamInfo.srcIpAddr = rs.srcIpAddress;
            streamInfo.dstIpAddr = rs.dstIpAddress;
            streamInfo.dstPort = rs.dstPort;

            std::string fileTemplate = ls.enhancedFileDeliveryTable.fileTemplate;

            // find mpd
            bool findMpd = false;
            for (const auto& rep : mpd.representations) {
                std::string mediaFileName = rep.mediaFileName;

                // normalize
                size_t pos = mediaFileName.find("$Number$");
                if (pos != std::string::npos) {
                    mediaFileName.replace(pos, 8, "$TOI$");
                }
                pos = fileTemplate.find("$Number$");
                if (pos != std::string::npos) {
                    fileTemplate.replace(pos, 8, "$TOI$");
                }

                if (mediaFileName == fileTemplate) {
                    streamInfo.contentType = rep.contentType;

                    for (const auto& item : ls.enhancedFileDeliveryTable.fileDeliveryTable) {
                        if (item.contentLocation == rep.initializationFileName) {
                            streamInfo.hasInitToi = true;
                            streamInfo.initToi = item.toi;
                            break;
                        }
                    }
                    findMpd = true;
                    break;
                }
            }

            if (!findMpd) {
                continue;
            }

            auto it = mapStream.find(ls.transportSessionId);
            if (it != mapStream.end()) {
                it->second.fileName = streamInfo.fileName;
                it->second.srcIpAddr = streamInfo.srcIpAddr;
                it->second.dstIpAddr = streamInfo.dstIpAddr;
                it->second.dstPort = streamInfo.dstPort;
                it->second.contentType = streamInfo.contentType;
                if (streamInfo.hasInitToi) {
                    it->second.hasInitToi = true;
                    it->second.initToi = streamInfo.initToi;
                }
                else {
                    it->second.hasInitToi = false;
                }
            }
            else {
                // find the available idx
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
                streamInfo.idx = i;
                mapStream[ls.transportSessionId] = streamInfo;
            }

            tsiList.push_back(ls.transportSessionId);
        }
    }

    // remove not found tsi
    for (auto it = mapStream.begin(); it != mapStream.end();) {
        if (std::find(tsiList.begin(), tsiList.end(), it->first) == tsiList.end()) {
            it = mapStream.erase(it);
        }
        else {
            ++it;
        }
    }


    if (demuxerHandler != nullptr && *demuxerHandler != nullptr) {
        (*demuxerHandler)->onPmt(*this);
    }
}

std::optional<std::reference_wrapper<StreamInfo>> Service::findStream(uint32_t transportSessionId) {
    auto it = mapStream.find(transportSessionId);
    if (it != mapStream.end()) {
        return it->second;
    }
    return {};
}