#pragma once
#include <string>
#include <list>
#include <optional>

enum class ContentType {
    VIDEO,
    AUDIO,
    SUBTITLE,
    UNKNOWN,
};
class MPD {
public:

    class Representation {
    public:
        ContentType contentType{ ContentType::UNKNOWN };
        std::string codecs;
        std::string id;

        std::string lang;

        uint32_t bandwidth;
        uint32_t width;
        uint32_t height;
        uint32_t audioSamplingRate;
        uint32_t duration;

        std::string initializationFileName;
        std::string mediaFileName;

        std::list<std::string> contentProtection;
    };

    std::list<Representation> representations;

    bool parse(const std::string& xml);
    std::optional<std::reference_wrapper<Representation>> findRepresentationByMediaFileName(const std::string& fileName);
    std::optional<std::reference_wrapper<Representation>> findRepresentationByInitFileName(const std::string& fileName);
};