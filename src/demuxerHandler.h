#pragma once
#include "RouteObject.h"

class DemuxerHandler {
public:
	virtual void onPmt(const std::unordered_map<uint32_t, RouteObject>& objects) {};
	virtual void onStreamData(const std::vector<StreamPacket>& chunks, const RouteObject& object, const std::vector<uint8_t>& decryptedMP4, uint64_t& baseDts, uint32_t transportObjectId) {};

};