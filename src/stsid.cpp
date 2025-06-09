#include "stsid.h"
#include "pugixml.hpp"

bool STSID::parse(const std::string& xml)
{
    rsList.clear();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(xml.data(), xml.size());

    for (const pugi::xml_node& rsNode : doc.child("S-TSID").children("RS")) {
        RS rs;
        if (!rsNode.attribute("sIpAddr") || !rsNode.attribute("dIpAddr") || !rsNode.attribute("dport")) {
            continue;
        }
        rs.srcIpAddress = std::stoul(rsNode.attribute("sIpAddr").value());
        rs.dstIpAddress = std::stoul(rsNode.attribute("dIpAddr").value());
        rs.dstPort = std::stoul(rsNode.attribute("dport").value());

        for (const pugi::xml_node& lsNode : rsNode.children("LS")) {
            LS ls;
            if (!lsNode.attribute("tsi")) {
                continue;
            }
            ls.transportSessionId = std::stoul(lsNode.attribute("tsi").value());

            for (const pugi::xml_node& srcFlowNode : lsNode.children("SrcFlow")) {
                const pugi::xml_node& efdtNode = srcFlowNode.child("EFDT");
                if (!efdtNode) {
                    continue;
                }

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

            rs.lsList.push_back(ls);
        }
        rsList.push_back(rs);
    }

    return false;
}
