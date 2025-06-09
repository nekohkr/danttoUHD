#include "mpd.h"
#include "pugixml.hpp"

bool MPD::parse(const std::string& xml)
{
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
                        rep.contentType = ContentType::VIDEO;
                    }
                    else if (contentType == "audio") {
                        rep.contentType = ContentType::AUDIO;
                    }
                    else if (contentType == "text") {
                        rep.contentType = ContentType::SUBTITLE;
                    }
                    else {
                        rep.contentType = ContentType::UNKNOWN;
                    }
                }
                else {
                    if (adaptationSetNode.attribute("mimeType")) {
                        contentType = adaptationSetNode.attribute("mimeType").value();
                        if (contentType == "video/mp4") {
                            rep.contentType = ContentType::VIDEO;
                        }
                        else if (contentType == "audio/mp4") {
                            rep.contentType = ContentType::AUDIO;
                        }
                        else if (contentType == "application/mp4") {
                            rep.contentType = ContentType::SUBTITLE;
                        }
                        else {
                            rep.contentType = ContentType::UNKNOWN;
                        }
                    }
                    else {
                        contentType = representationNode.attribute("mimeType").value();
                        if (contentType == "video/mp4") {
                            rep.contentType = ContentType::VIDEO;
                        }
                        else if (contentType == "audio/mp4") {
                            rep.contentType = ContentType::AUDIO;
                        }
                        else if (contentType == "application/mp4") {
                            rep.contentType = ContentType::SUBTITLE;
                        }
                        else {
                            rep.contentType = ContentType::UNKNOWN;
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

std::optional<std::reference_wrapper<MPD::Representation>> MPD::findRepresentationByMediaFileName(const std::string& fileName)
{
    for (auto& rep : representations) {
        if (rep.mediaFileName == fileName) {
            return std::ref(rep);
        }
    }
    return std::optional<std::reference_wrapper<Representation>>();
}

std::optional<std::reference_wrapper<MPD::Representation>> MPD::findRepresentationByInitFileName(const std::string& fileName)
{
    for (auto& rep : representations) {
        if (rep.initializationFileName == fileName) {
            return std::ref(rep);
        }
    }
    return std::optional<std::reference_wrapper<Representation>>();
}
