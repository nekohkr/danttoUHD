#include "demuxer.h"
#include <iomanip>
#include <sstream>
#include <winsock2.h>
#include "stream.h"
#include "ipv4.h"
#include "udp.h"
#include "mp4Processor.h"
#include "pugixml.hpp"
#include "atsc3.h"
#include "atsc3BasebandPacket.h"
#include "atsc3Table.h"
#include "service.h"
#include "mmt.h"
#include <unordered_set>
#include "rescale.h"

namespace atsc3 {

DemuxStatus Demuxer::demux(const std::vector<uint8_t>& input) {
    const auto callback = [this](const LgContainer& lgContainer) {
        // continuity counter check
        auto it = mapCC.find(lgContainer.plpId);
        if (it == mapCC.end()) {
            mapCC[lgContainer.plpId] = lgContainer.cc;
        }
        else {
            uint8_t expectedCC = it->second + 1;
            if (lgContainer.cc != expectedCC) {
                fprintf(stderr,
                    "[DROP] plpId=%u, expected_cc=%u, actual_cc=%u\n",
                    lgContainer.plpId, expectedCC, lgContainer.cc);
            }
            it->second = lgContainer.cc;
        }

        if (lgContainer.errorMode == true && lgContainer.error == true) {
            fprintf(stderr, "[ERROR] plpId=%u\n", lgContainer.plpId);
        }

        Common::ReadStream s(lgContainer.payload);

        atsc3::Atsc3BasebandPacket bbPacket;
        if (!bbPacket.unpack(s)) {
            return;
        }

        uint32_t alpOffset = 0;
        if (!alpAligned) {
            alpOffset = bbPacket.baseField.pointer;
            if (bbPacket.baseField.pointer == 8191) {
                return;
            }
            alpAligned = true;
        }

        Common::ReadStream bbPayloadStream(bbPacket.payload);
        if (bbPayloadStream.leftBytes() < alpOffset) {
            alpAligned = false;
            alpBuffer.clear();
            return;
        }
        bbPayloadStream.skip(alpOffset);

        size_t oldSize = alpBuffer.size();
        alpBuffer.resize(alpBuffer.size() + bbPayloadStream.leftBytes());
        bbPayloadStream.read(alpBuffer.data() + oldSize, bbPayloadStream.leftBytes());

        while (alpBuffer.size() > 2) {
            Common::ReadStream alpStream(alpBuffer);
            atsc3::Atsc3Alp alp;
            atsc3::UnpackResult result = alp.unpack(alpStream);

            if (result == atsc3::UnpackResult::NotEnoughData) {
                return;
            }

            atsc3::Atsc3AlpPacketType packetType = static_cast<atsc3::Atsc3AlpPacketType>(alp.packetType);
            if (packetType == atsc3::Atsc3AlpPacketType::IPv4) {
                pcapWriter.writePacket(alp.payload);

                Common::ReadStream payloadStream(alp.payload);
                if (!processIpUdp(payloadStream)) {
                    alpAligned = false;
                    alpBuffer.clear();
                    return;
                }
            }

            alpBuffer.erase(alpBuffer.begin(), alpBuffer.begin() + (alpBuffer.size() - alpStream.leftBytes()));
        }
    };

    lgContainerUnpacker.addBuffer(input);
    lgContainerUnpacker.unpack(callback);

    return DemuxStatus::Ok;
}

void Demuxer::setHandler(DemuxerHandler* handler) {
    this->handler = handler;
}


bool Demuxer::processIpUdp(Common::ReadStream& stream) {
    IPv4Header ipv4;
    if (!ipv4.unpack(stream)) {
        return false;
    }

    UDPHeader udp;
    if (!udp.unpack(stream)) {
        return false;
    }

    size_t leftBytes = stream.leftBytes();
    if (udp.length != leftBytes + 8) {
        return false;
    }

    if (ipv4.dstIpAddr == inet_addr("224.0.23.60") && udp.dstPort == 4937) {
        processLls(stream);
        return true;
    }

    auto service = serviceManager.findServiceByIp(ipv4.dstIpAddr, udp.dstPort);
    if (service) {
        return service->processPacket(stream);
    }

    return true;
}

bool Demuxer::processLls(Common::ReadStream& stream) {
    atsc3::Atsc3LowLevelSignaling lls;
    lls.unpack(stream);

    switch (lls.tableId) {
    case atsc3::Atsc3ServiceListTable::kTableId:
    {
        atsc3::Atsc3ServiceListTable slt;
        slt.unpack(lls.payload);
        processSlt(slt);
        break;
    }
    }

    return true;
}

bool Demuxer::processSlt(const atsc3::Atsc3ServiceListTable& slt) {
    serviceManager.bsid = slt.bsid;

    std::unordered_set<uint32_t> serviceIds;
    for (const auto& service : slt.services) {
        serviceIds.insert(service.serviceId);
    }

    for (auto it = serviceManager.services.begin(); it != serviceManager.services.end(); ) {
        if (serviceIds.find(it->get()->serviceId) == serviceIds.end()) {
            it = serviceManager.services.erase(it);
        }
        else {
            ++it;
        }
    }

    for (const auto& service : slt.services) {
        auto it = std::find_if(
            serviceManager.services.begin(),
            serviceManager.services.end(),
            [&](auto& s) { return s->serviceId == service.serviceId; }
        );
        if (it != serviceManager.services.end()) {
            (*it)->serviceCategory = service.serviceCategory;
            (*it)->shortServiceName = service.shortServiceName;
            (*it)->slsProtocol = service.slsProtocol;
            (*it)->slsMajorProtocolVersion = service.slsMajorProtocolVersion;
            (*it)->slsMinorProtocolVersion = service.slsMinorProtocolVersion;
            (*it)->slsDestinationIpAddress = service.slsDestinationIpAddress;
            (*it)->slsDestinationUdpPort = service.slsDestinationUdpPort;
            (*it)->slsSourceIpAddress = service.slsSourceIpAddress;
        }
        else {
            std::shared_ptr<Service> newService = std::make_shared<Service>(handler);
            newService->serviceId = service.serviceId;
            newService->serviceCategory = service.serviceCategory;
            newService->shortServiceName = service.shortServiceName;
            newService->slsProtocol = service.slsProtocol;
            newService->slsMajorProtocolVersion = service.slsMajorProtocolVersion;
            newService->slsMinorProtocolVersion = service.slsMinorProtocolVersion;
            newService->slsDestinationIpAddress = service.slsDestinationIpAddress;
            newService->slsDestinationUdpPort = service.slsDestinationUdpPort;
            newService->slsSourceIpAddress = service.slsSourceIpAddress;
            serviceManager.AddService(newService);
        }
    }

    if (handler) {
        handler->onSlt(serviceManager);
    }

    return true;
}

}