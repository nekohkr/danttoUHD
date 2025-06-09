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
#include "bbPacket.h"
#include "alp.h"
#include "lct.h"
#include "lls.h"
#include "service.h"

DemuxStatus Demuxer::demux(const std::vector<uint8_t>& input) {
    const auto callback = [this](const LgContainer& lgContainer) {
        Common::ReadStream s(lgContainer.payload);

        ATSC3::BasebandPacket basebandPacket;
        if (!basebandPacket.unpack(s)) {
            return;
        }

        uint32_t alpOffset = 0;
        if (!alpAligned) {
            alpOffset = basebandPacket.baseField.pointer;
            if (basebandPacket.baseField.pointer == 8191) {
                return;
            }
            alpAligned = true;
        }

        Common::ReadStream bbPayloadStream(basebandPacket.payload);
        bbPayloadStream.skip(alpOffset);

        size_t oldSize = alpBuffer.size();
        alpBuffer.resize(alpBuffer.size() + bbPayloadStream.leftBytes());
        bbPayloadStream.read(alpBuffer.data() + oldSize, bbPayloadStream.leftBytes());

        while (alpBuffer.size() > 2) {
            Common::ReadStream alpStream(alpBuffer);
            ATSC3::ALP alp;
            ATSC3::UnpackResult result = alp.unpack(alpStream);

            if (result == ATSC3::UnpackResult::NotEnoughData) {
                return;
            }

            ATSC3::AlpPacketType packetType = static_cast<ATSC3::AlpPacketType>(alp.packetType);
            if (packetType == ATSC3::AlpPacketType::IPv4) {
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

void Demuxer::setHandler(DemuxerHandler* handler)
{
    this->handler = handler;
}

bool Demuxer::processIpUdp(Common::ReadStream& stream)
{
    IPv4Header ipv4;
    if (!ipv4.unpack(stream)) {
        return false;
    }
    this->ipv4 = ipv4;

    UDPHeader udp;
    if (!udp.unpack(stream)) {
        return false;
    }

    size_t leftBytes = stream.leftBytes();
    if (udp.length != leftBytes + 8) {
        return false;
    }

    if (ipv4.dstIpAddr == inet_addr("224.0.23.60") && udp.dstPort == 4937) {
        processLLS(stream);
        return true;
    }

    auto service = serviceManager.findServiceByIp(ipv4.dstIpAddr, udp.dstPort);
    
    if (service->get().serviceId != 7) {
        return true;
    }
    if (service) {
        processALC(stream, *service);
    }

    return true;
}

bool Demuxer::processLLS(Common::ReadStream& stream)
{
    ATSC3::LLS lls;
    lls.unpack(stream);

    switch (lls.tableId) {
    case 0x01: // SLT
    {
        sltHandler.process(lls.payload);
        if (handler) {
            handler->onSlt(serviceManager);
        }
        break;
    }
    }

    return true;
}

bool Demuxer::processALC(Common::ReadStream& stream, Service& service)
{
    ATSC3::LCT lct;
    if (!lct.unpack(stream)) {
        return false;
    }

    uint16_t sbn = stream.getBe16U();
    uint16_t esid = stream.getBe16U();

    size_t size = stream.leftBytes();
    std::vector<uint8_t> buffer(size);
    stream.read(buffer.data(), size);

    service.onALP(lct, buffer);
    return true;
}
