#include "service.h"
#include <map>
#include <chrono>
#include "pugixml.hpp"
#include "mp4Processor.h"
#include "stsid.h"
#include "rescale.h"

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

std::string normalizeFileName(std::string input, const std::string& representationId) {
    size_t pos = input.find("$Number$");
    if (pos != std::string::npos) {
        input.replace(pos, 8, "$TOI$");
    }
    pos = input.find("$RepresentationID$");
    if (pos != std::string::npos) {
        input.replace(pos, 18, representationId);
    }

    return input;
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
    return true;
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

    if (serviceCategory == ATSC3::ServiceCategory::EsgService) {
        auto ls = stsid.findLS(object.transportSessionId);
        if (!ls) {
            return true;
        }

        std::string xml(object.buffer.begin(), object.buffer.end());

        if (ls->get().contentInfo == "") {

        }
    }
    else if (serviceCategory == ATSC3::ServiceCategory::LinearAVService ||
        serviceCategory == ATSC3::ServiceCategory::LinearAudioOnlyService) {
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
                MP4ConfigParser::parse(stream->get().initMP4, stream->get().mp4Config);
                return true;
            }
        }

        std::vector<uint8_t> input;
        std::vector<uint8_t> decryptedMP4;
        std::vector<StreamPacket> packets;

        input.insert(input.end(), stream->get().initMP4.begin(), stream->get().initMP4.end());
        input.insert(input.end(), object.buffer.begin(), object.buffer.end());

        if (stream->get().contentType == ContentType::VIDEO ||
            stream->get().contentType == ContentType::AUDIO ||
            stream->get().contentType == ContentType::SUBTITLE) {
            if (!mp4Processor.process(input, packets, decryptedMP4)) {
                return false;
            }

            if (packets.size() == 0) {
                return false;
            }

            if (baseDts == 0) {
                AVRational r = { 1, static_cast<int>(stream->get().mp4Config.timescale) };
                AVRational ts = { 1, 90000 };
                baseDts = av_rescale_q(packets.front().dts, r, ts);

                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                baseTs = ms;
            }

            if (demuxerHandler != nullptr && *demuxerHandler != nullptr) {
                AVRational r = { 1, static_cast<int>(stream->get().mp4Config.timescale) };
                AVRational ts = { 1, 90000 };
                int64_t rescaledDts = av_rescale_q(packets[0].dts, r, ts);
                int64_t baseDtsTimestamp = baseTs + (rescaledDts - baseDts) / 90;
                (*demuxerHandler)->onStreamData(*this, stream->get(), packets, decryptedMP4, baseDtsTimestamp);
            }
        }
    }
    return true;
}

bool Service::processSLS(const std::unordered_map<std::string, std::string>& files)
{
    for (const auto& file : files) {
        if (file.first == "stsid.xml") {
            stsid.parse(file.second);
        }
        else if (file.first == "mpd.xml") {
            mpd.parse(file.second);
        }
    }

    updateStreamMap();
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

            std::string fileTemplate = normalizeFileName(ls.enhancedFileDeliveryTable.fileTemplate, "");

            // find mpd
            bool findMpd = false;
            for (const auto& rep : mpd.representations) {
                std::string mediaFileName = normalizeFileName(rep.mediaFileName, rep.id);

                size_t pos = mediaFileName.find("$Number$");
                if (pos != std::string::npos) {
                    mediaFileName.replace(pos, 8, "$TOI$");
                }
                pos = mediaFileName.find("$RepresentationID$");
                if (pos != std::string::npos) {
                    mediaFileName.replace(pos, 18, rep.id);
                }
                pos = fileTemplate.find("$Number$");
                if (pos != std::string::npos) {
                    fileTemplate.replace(pos, 8, "$TOI$");
                }

                if (mediaFileName == fileTemplate) {
                    streamInfo.language = rep.lang;
                    streamInfo.contentType = rep.contentType;

                    for (const auto& item : ls.enhancedFileDeliveryTable.fileDeliveryTable) {
                        std::string initializationFileName = normalizeFileName(rep.initializationFileName, rep.id);

                        if (item.contentLocation == initializationFileName) {
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