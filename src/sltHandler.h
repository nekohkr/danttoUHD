#pragma once
#include <string>

class ServiceManager;
class DemuxerHandler;

class SltHandler {
public:
    SltHandler(ServiceManager& sm, DemuxerHandler** handler)
        : serviceManager(sm), handler(handler) {}
    void process(std::string xml);


private:
    ServiceManager& serviceManager;
    DemuxerHandler** handler;

};