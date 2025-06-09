#include "sltHandler.h"
#include "pugixml.hpp"
#include "serviceManager.h"
#include <WS2tcpip.h>
#include <unordered_set>

namespace {

uint32_t ipToUint(const std::string& ip) {
    uint32_t addr = 0;
    inet_pton(AF_INET, ip.c_str(), &addr);
    return addr;
}

}

void SltHandler::process(std::string xml)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(xml.data(), xml.size());
    if (!result) {
        return;
    }

    std::unordered_set<uint32_t> xmlServiceIds;
    for (pugi::xml_node serviceNode : doc.child("SLT").children("Service")) {
        xmlServiceIds.insert(serviceNode.attribute("serviceId").as_uint());
    }

    for (auto it = serviceManager.services.begin(); it != serviceManager.services.end(); ) {
        if (xmlServiceIds.find(it->serviceId) == xmlServiceIds.end()) {
            it = serviceManager.services.erase(it);
        }
        else {
            ++it;
        }
    }

    if (doc.child("SLT").attribute("bsid")) {
        serviceManager.bsid = doc.child("SLT").attribute("bsid").as_uint();
    }

    for (pugi::xml_node serviceNode : doc.child("SLT").children("Service")) {
        Service info(handler);
        info.serviceId = serviceNode.attribute("serviceId").as_uint();
        info.serviceCategory = static_cast<ATSC3::ServiceCategory>(serviceNode.attribute("serviceCategory").as_uint());
        info.shortServiceName = serviceNode.attribute("shortServiceName").as_string();

        auto bss = serviceNode.child("BroadcastSvcSignaling");
        if (bss) {
            info.slsProtocol = bss.attribute("slsProtocol").as_uint();
            info.slsMajorProtocolVersion = bss.attribute("slsMajorProtocolVersion").as_uint();
            info.slsMinorProtocolVersion = bss.attribute("slsMinorProtocolVersion").as_uint();
            info.slsDestinationIpAddress = ipToUint(bss.attribute("slsDestinationIpAddress").as_string());
            info.slsDestinationUdpPort = bss.attribute("slsDestinationUdpPort").as_uint();
            info.slsSourceIpAddress = ipToUint(bss.attribute("slsSourceIpAddress").as_string());
        }

        auto it = std::find_if(
            serviceManager.services.begin(),
            serviceManager.services.end(),
            [&](const Service& s) { return s.serviceId == info.serviceId; }
        );
        if (it != serviceManager.services.end()) {
            (*it).serviceCategory = info.serviceCategory;
            (*it).shortServiceName = info.shortServiceName;
            (*it).slsProtocol = info.slsProtocol;
            (*it).slsMajorProtocolVersion = info.slsMajorProtocolVersion;
            (*it).slsMinorProtocolVersion = info.slsMinorProtocolVersion;
            (*it).slsDestinationIpAddress = info.slsDestinationIpAddress;
            (*it).slsDestinationUdpPort = info.slsDestinationUdpPort;
            (*it).slsSourceIpAddress = info.slsSourceIpAddress;
        }
        else {
            serviceManager.AddService(info);
        }
    }

}
