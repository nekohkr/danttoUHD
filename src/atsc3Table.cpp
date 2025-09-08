#include "atsc3Table.h"
#include "pugixml.hpp"
#include <unordered_set>
#include "ip.h"

namespace atsc3 {

bool Atsc3ServiceListTable::unpack(const std::string& xml)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(xml.data(), xml.size());
    if (!result) {
        return false;
    }

    if (doc.child("SLT").attribute("bsid")) {
        bsid = doc.child("SLT").attribute("bsid").as_uint();
    }

    for (pugi::xml_node serviceNode : doc.child("SLT").children("Service")) {
        struct Service service;
        service.serviceId = serviceNode.attribute("serviceId").as_uint();
        service.serviceCategory = static_cast<atsc3::Atsc3ServiceCategory>(serviceNode.attribute("serviceCategory").as_uint());
        service.shortServiceName = serviceNode.attribute("shortServiceName").as_string();

        auto bss = serviceNode.child("BroadcastSvcSignaling");
        if (bss) {
            service.slsProtocol = bss.attribute("slsProtocol").as_uint();
            service.slsMajorProtocolVersion = bss.attribute("slsMajorProtocolVersion").as_uint();
            service.slsMinorProtocolVersion = bss.attribute("slsMinorProtocolVersion").as_uint();
            service.slsDestinationIpAddress = Common::ipToUint(bss.attribute("slsDestinationIpAddress").as_string());
            service.slsDestinationUdpPort = bss.attribute("slsDestinationUdpPort").as_uint();
            service.slsSourceIpAddress = Common::ipToUint(bss.attribute("slsSourceIpAddress").as_string());
        }

        services.push_back(service);
    }

    return true;
}

}
