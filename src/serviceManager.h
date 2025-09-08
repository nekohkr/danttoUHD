#pragma once
#include <list>
#include "service.h"
#include <optional>

namespace atsc3 {

class Demuxer;
class ServiceManager {
public:
    std::shared_ptr<Service> findServiceById(uint32_t serviceId);
    std::shared_ptr<Service> findServiceByIp(uint32_t dstIp, uint16_t dstPort);
    bool AddService(std::shared_ptr<Service> service);

public:
    std::list<std::shared_ptr<Service>> services;
    std::unordered_map<uint16_t, uint16_t> mapServiceIdToPmtPid;
    uint32_t bsid{ 0 };
};

}