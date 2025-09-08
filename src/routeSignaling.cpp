#include "routeSignaling.h"
#include "pugixml.hpp"
#include "ip.h"

namespace atsc3 {

bool RouteMpd::unpack(const std::string& xml) {
    representations.clear();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(xml.data(), xml.size());

    for (const pugi::xml_node& periodNode : doc.child("MPD").children("Period")) {
        for (const pugi::xml_node& adaptationSetNode : periodNode.children("AdaptationSet")) {
            for (const pugi::xml_node& representationNode : adaptationSetNode.children("Representation")) {
                if (!representationNode.child("SegmentTemplate")) {
                    continue;
                }

                Representation rep;
                for (const pugi::xml_node& representationNode : adaptationSetNode.children("ContentProtection")) {
                    rep.contentProtection.push_back(representationNode.attribute("value").value());
                }

                if (adaptationSetNode.attribute("lang")) {
                    rep.lang = adaptationSetNode.attribute("lang").value();
                }

                std::string contentType = adaptationSetNode.attribute("contentType").value();
                if (adaptationSetNode.attribute("contentType")) {
                    if (contentType == "video") {
                        rep.contentType = RouteContentType::VIDEO;
                    }
                    else if (contentType == "audio") {
                        rep.contentType = RouteContentType::AUDIO;
                    }
                    else if (contentType == "text") {
                        rep.contentType = RouteContentType::SUBTITLE;
                    }
                    else {
                        rep.contentType = RouteContentType::UNKNOWN;
                    }
                }
                else {
                    if (adaptationSetNode.attribute("mimeType")) {
                        contentType = adaptationSetNode.attribute("mimeType").value();
                        if (contentType == "video/mp4") {
                            rep.contentType = RouteContentType::VIDEO;
                        }
                        else if (contentType == "audio/mp4") {
                            rep.contentType = RouteContentType::AUDIO;
                        }
                        else if (contentType == "application/mp4") {
                            rep.contentType = RouteContentType::SUBTITLE;
                        }
                        else {
                            rep.contentType = RouteContentType::UNKNOWN;
                        }
                    }
                    else {
                        contentType = representationNode.attribute("mimeType").value();
                        if (contentType == "video/mp4") {
                            rep.contentType = RouteContentType::VIDEO;
                        }
                        else if (contentType == "audio/mp4") {
                            rep.contentType = RouteContentType::AUDIO;
                        }
                        else if (contentType == "application/mp4") {
                            rep.contentType = RouteContentType::SUBTITLE;
                        }
                        else {
                            rep.contentType = RouteContentType::UNKNOWN;
                        }
                    }
                }

                if (representationNode.attribute("id")) {
                    rep.id = representationNode.attribute("id").value();
                }
                if (representationNode.attribute("bandwidth")) {
                    rep.bandwidth = std::stoul(representationNode.attribute("bandwidth").value());
                }
                if (representationNode.attribute("codecs")) {
                    rep.codecs = representationNode.attribute("codecs").value();
                }
                if (representationNode.attribute("audioSamplingRate")) {
                    rep.audioSamplingRate = std::stoul(representationNode.attribute("audioSamplingRate").value());
                }

                rep.mediaFileName = representationNode.child("SegmentTemplate").attribute("media").value();
                if (representationNode.child("SegmentTemplate").attribute("initialization")) {
                    rep.initializationFileName = representationNode.child("SegmentTemplate").attribute("initialization").value();
                }

                if (representationNode.child("SegmentTemplate").attribute("duration")) {
                    rep.duration = std::stoul(representationNode.child("SegmentTemplate").attribute("duration").value());
                }

                representations.push_back(rep);
            }
        }
    }
    return true;
}

std::optional<std::reference_wrapper<struct RouteMpd::Representation>> RouteMpd::findRepresentationByMediaFileName(const std::string& fileName)
{
    for (auto& rep : representations) {
        if (rep.mediaFileName == fileName) {
            return std::ref(rep);
        }
    }
    return std::optional<std::reference_wrapper<struct Representation>>();
}

std::optional<std::reference_wrapper<struct RouteMpd::Representation>> RouteMpd::findRepresentationByInitFileName(const std::string& fileName)
{
    for (auto& rep : representations) {
        if (rep.initializationFileName == fileName) {
            return std::ref(rep);
        }
    }
    return std::optional<std::reference_wrapper<struct Representation>>();
}

bool RouteStsid::unpack(const std::string& xml) {
    rsList.clear();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(xml.data(), xml.size());

    for (const pugi::xml_node& rsNode : doc.child("S-TSID").children("RS")) {
        RS rs;
        if (!rsNode.attribute("sIpAddr") || !rsNode.attribute("dIpAddr") || !rsNode.attribute("dport")) {
            continue;
        }
        rs.srcIpAddress = Common::ipToUint(rsNode.attribute("sIpAddr").value());
        rs.dstIpAddress = Common::ipToUint(rsNode.attribute("dIpAddr").value());
        rs.dstPort = std::stoul(rsNode.attribute("dport").value());

        for (const pugi::xml_node& lsNode : rsNode.children("LS")) {
            LS ls;
            if (!lsNode.attribute("tsi")) {
                continue;
            }
            ls.transportSessionId = std::stoul(lsNode.attribute("tsi").value());

            for (const pugi::xml_node& srcFlowNode : lsNode.children("SrcFlow")) {

                const pugi::xml_node& contentInfoNode = srcFlowNode.child("ContentInfo");
                if (contentInfoNode) {
                    ls.contentInfo = contentInfoNode.first_child().value();
                }
                const pugi::xml_node& efdtNode = srcFlowNode.child("EFDT");
                if (efdtNode) {
                    ls.enhancedFileDeliveryTable.fileTemplate = efdtNode.child("FileTemplate").child_value();

                    const pugi::xml_node& fdtParametersNode = efdtNode.child("FDTParameters");
                    for (const pugi::xml_node& fileNode : fdtParametersNode.children("File")) {
                        struct FileDeliveryTableItem item;
                        item.contentLocation = fileNode.attribute("Content-Location").value();
                        item.contentType = fileNode.attribute("Content-Type").value();
                        item.toi = std::stoul(fileNode.attribute("TOI").value());
                        ls.enhancedFileDeliveryTable.fileDeliveryTable.push_back(item);
                    }
                }
            }

            rs.lsList.push_back(ls);
        }
        rsList.push_back(rs);
    }

    return true;
}

}