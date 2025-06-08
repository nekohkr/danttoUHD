#pragma once
#include <unordered_map>
#include <string>
#include "routeObject.h"
#include "ipv4.h"
#include "mp4Processor.h"
#include "demuxerHandler.h"
#include "lgContainer.h"
#include "stream.h"

enum class DemuxStatus {
	Ok = 0x0000,
	NotEnoughBuffer = 0x1000,
	NotValidBBFrame = 0x1001,
	WattingForEcm = 0x1002,
	Error = 0x2000,
};

class Demuxer {
public:
	bool init();
	DemuxStatus demux(const std::vector<uint8_t>& input);
	bool isVaildBBFrame(Common::ReadStream& stream);
	void setHandler(DemuxerHandler* handler);
	void printStatistics() const;

	IPv4Header ipv4;
private:
	bool processIpUdp(Common::ReadStream& stream);
	bool processLLS(Common::ReadStream& stream);
	bool processMMTP(Common::ReadStream& stream);
	bool processLCT(Common::ReadStream& stream);
	bool processRouteObject(RouteObject& alcStream, uint32_t transportObjectId);
	bool processConfigs();

	std::unordered_map<uint32_t, RouteObject> routeObjects;
	
	std::vector<uint8_t> alpBuffer;
	bool isFirstPacket = true;
	bool alpAligned{ false };

	MP4Processor mp4Processor;

	std::map<std::string, std::string> configFiles;
	std::map<uint32_t, std::string> stsid;
	DemuxerHandler* handler;

	LgContainerUnpacker lgContainerUnpacker;
};

