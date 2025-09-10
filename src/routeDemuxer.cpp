#include "routeDemuxer.h"

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

namespace atsc3  {

bool RouteDemuxer::processSls(const std::unordered_map<std::string, std::string>& files) {
    for (const auto& file : files) {
        if (file.first == "stsid.xml") {
            stsid.unpack(file.second);
        }
        else if (file.first == "mpd.xml") {
            mpd.unpack(file.second);
        }
    }

    updateStreamMap();
    return true;
}

void RouteDemuxer::updateStreamMap() {
    std::list<uint32_t> tsiList;

    for (const auto& rs : stsid.rsList) {
        for (const auto& ls : rs.lsList) {
            RouteStream stream;
            stream.transportSessionId = ls.transportSessionId;
            stream.fileName = ls.enhancedFileDeliveryTable.fileTemplate;
            stream.srcIpAddr = rs.srcIpAddress;
            stream.dstIpAddr = rs.dstIpAddress;
            stream.dstPort = rs.dstPort;

            std::string fileTemplate = normalizeFileName(ls.enhancedFileDeliveryTable.fileTemplate, "");

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
                    stream.language = rep.lang;
                    stream.contentType = rep.contentType;

                    for (const auto& item : ls.enhancedFileDeliveryTable.fileDeliveryTable) {
                        std::string initializationFileName = normalizeFileName(rep.initializationFileName, rep.id);

                        if (item.contentLocation == initializationFileName) {
                            stream.hasInitToi = true;
                            stream.initToi = item.toi;
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
                it->second.fileName = stream.fileName;
                it->second.srcIpAddr = stream.srcIpAddr;
                it->second.dstIpAddr = stream.dstIpAddr;
                it->second.dstPort = stream.dstPort;
                it->second.contentType = stream.contentType;
                if (stream.hasInitToi) {
                    it->second.hasInitToi = true;
                    it->second.initToi = stream.initToi;
                }
                else {
                    it->second.hasInitToi = false;
                }
            }
            else {
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
                stream.idx = i;
                stream.packetId = ls.transportSessionId;
                mapStream[ls.transportSessionId] = stream;
            }

            tsiList.push_back(ls.transportSessionId);
        }
    }

    for (auto it = mapStream.begin(); it != mapStream.end();) {
        if (std::find(tsiList.begin(), tsiList.end(), it->first) == tsiList.end()) {
            it = mapStream.erase(it);
        }
        else {
            ++it;
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

bool RouteDemuxer::processRouteObject(const RouteObject& object, uint32_t transportObjectId) {
    if (object.transportSessionId == 0) {
        // SLS
        std::string data(object.buffer.begin(), object.buffer.end());
        auto sls = parseMultipartRelated(data);
        processSls(sls);
    }
    else {
        if (serviceCategory == atsc3::Atsc3ServiceCategory::EsgService) {
            auto ls = stsid.findLS(object.transportSessionId);
            if (!ls) {
                return true;
            }

            std::string xml(object.buffer.begin(), object.buffer.end());
            // TODO
        }
        else if (serviceCategory == atsc3::Atsc3ServiceCategory::LinearAVService ||
            serviceCategory == atsc3::Atsc3ServiceCategory::LinearAudioOnlyService) {

            if (mapStream.find(object.transportSessionId) == mapStream.end()) {
                return true;
            }

            auto& stream = mapStream[object.transportSessionId];
            if (stream.hasInitToi) {
                if (stream.initToi != transportObjectId && !stream.initMP4.size()) {
                    return true;
                }

                if (stream.initToi == transportObjectId) {
                    stream.initMP4 = object.buffer;
                    return true;
                }
            }

            std::vector<uint8_t> input;
            std::vector<StreamPacket> packets;

            input.insert(input.end(), stream.initMP4.begin(), stream.initMP4.end());
            input.insert(input.end(), object.buffer.begin(), object.buffer.end());

            onMediaData(stream, input, stream.initMP4, 0);
        }
    }


    return true;
}

bool RouteDemuxer::processPacket(Common::ReadStream& stream) {
    RouteLayeredCodingTransport lct;
    if (!lct.unpack(stream)) {
        return false;
    }

    uint16_t sbn = stream.getBe16U();
    uint16_t esid = stream.getBe16U();

    size_t size = stream.leftBytes();
    std::vector<uint8_t> payload(size);
    stream.read(payload.data(), size);

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

}