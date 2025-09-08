#pragma once
#include <cstdint>
#include <string>
#include <optional>
#include <list>

namespace atsc3 {

class RouteSignaling {
public:
    virtual ~RouteSignaling() = default;
    virtual bool unpack(const std::string& xml) = 0;
};

enum class RouteContentType {
    VIDEO,
    AUDIO,
    SUBTITLE,
    UNKNOWN,
};

class RouteMpd : public RouteSignaling {
public:
    struct Representation {
        RouteContentType contentType{ RouteContentType::UNKNOWN };
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

    std::list<struct Representation> representations;

    bool unpack(const std::string& xml);
    std::optional<std::reference_wrapper<struct Representation>> findRepresentationByMediaFileName(const std::string& fileName);
    std::optional<std::reference_wrapper<struct Representation>> findRepresentationByInitFileName(const std::string& fileName);
};

class RouteStsid : public RouteSignaling {
public:
    struct FileDeliveryTableItem {
        uint32_t toi;
        std::string contentLocation;
        std::string contentType;
    };

    struct EnhancedFileDeliveryTable {
        std::string fileTemplate;
        std::list<struct FileDeliveryTableItem> fileDeliveryTable;
    };

    struct LS {
        uint32_t transportSessionId{ 0 };
        struct EnhancedFileDeliveryTable enhancedFileDeliveryTable;
        std::string contentInfo;

        std::optional<std::reference_wrapper<struct FileDeliveryTableItem>> findFileDelivery(uint32_t toi) {
            for (auto& item : enhancedFileDeliveryTable.fileDeliveryTable) {
                if (item.toi == toi) {
                    return item;
                }
            }
            return {};
        }
    };

    struct RS {
        uint32_t srcIpAddress{ 0 };
        uint32_t dstIpAddress{ 0 };
        uint16_t dstPort{ 0 };

        std::list<struct LS> lsList;

        std::optional<std::reference_wrapper<LS>> findLS(uint32_t transportSessionId) {
            for (auto& ls : lsList) {
                if (ls.transportSessionId == transportSessionId) {
                    return ls;
                }
            }
            return {};
        }

    };

    std::optional<std::reference_wrapper<struct RS>> findRSByIP(uint32_t srcIpAddress, uint32_t dstIpAddress, uint16_t dstPort) {
        for (auto& rs : rsList) {
            if (rs.srcIpAddress == srcIpAddress && rs.dstIpAddress == dstIpAddress && rs.dstPort == dstPort) {
                return rs;
            }
        }
        return {};
    }
    std::optional<std::reference_wrapper<struct LS>> findLS(uint32_t tsi) {
        for (auto& rs : rsList) {
            return rs.findLS(tsi);
        }
        return {};
    }

    bool unpack(const std::string& xml);

    std::list<struct RS> rsList;
};

}