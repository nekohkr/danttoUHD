#pragma once
#include <list>
#include "service.h"
#include <optional>

class Demuxer;

class ServiceManager {
public:
    std::list<Service> services;
    std::optional<std::reference_wrapper<Service>> findServiceById(uint32_t serviceId);
    std::optional<std::reference_wrapper<Service>> findServiceByIp(uint32_t dstIp, uint16_t dstPort);
    bool AddService(const Service& service);
    std::unordered_map<uint16_t, uint16_t> mapServiceIdToPmtPid;
    uint32_t bsid{ 0 };
};