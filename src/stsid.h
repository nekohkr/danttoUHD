#pragma once
#include <cstdint>
#include <string>
#include <list>
#include <optional>

class STSID {
public:
    struct FileDeliveryTableItem {
        uint32_t toi;
        std::string contentLocation;
        std::string contentType;
    };

    class EnhancedFileDeliveryTable {
    public:
        std::string fileTemplate;
        std::list<struct FileDeliveryTableItem> fileDeliveryTable;
    };

    class LS {
    public:
        uint32_t transportSessionId{ 0 };
        EnhancedFileDeliveryTable enhancedFileDeliveryTable;
        std::string contentInfo;
        std::optional<std::reference_wrapper<FileDeliveryTableItem>> findFileDelivery(uint32_t toi) {
            for (auto& item : enhancedFileDeliveryTable.fileDeliveryTable) {
                if (item.toi == toi) {
                    return item;
                }
            }
            return {};
        }
    };

    class RS {
    public:
        uint32_t srcIpAddress{ 0 };
        uint32_t dstIpAddress{ 0 };
        uint16_t dstPort{ 0 };

        std::list<LS> lsList;


        std::optional<std::reference_wrapper<LS>> findLS(uint32_t transportSessionId) {
            for (auto& ls : lsList) {
                if (ls.transportSessionId == transportSessionId) {
                    return ls;
                }
            }
            return {};
        }

    };

    std::list<RS> rsList;

    std::optional<std::reference_wrapper<RS>> findRSByIP(uint32_t srcIpAddress, uint32_t dstIpAddress, uint16_t dstPort) {
        for (auto& rs : rsList) {
            if (rs.srcIpAddress == srcIpAddress && rs.dstIpAddress == dstIpAddress && rs.dstPort == dstPort) {
                return rs;
            }
        }
        return {};
    }
    std::optional<std::reference_wrapper<LS>> findLS(uint32_t tsi) {
        for (auto& rs : rsList) {
            return rs.findLS(tsi);
        }
        return {};
    }

    bool parse(const std::string& xml);

};