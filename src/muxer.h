#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <tsduck.h>
#include "streamPacket.h"
#include "demuxerHandler.h"
#include "ac3Encoder.h"

struct AVCodecContext;

class Muxer : public DemuxerHandler {
public:
	using OutputCallback = std::function<void(const uint8_t*, size_t)>;
	void setOutputCallback(OutputCallback cb);

private:
	virtual void onPmt(const std::unordered_map<uint32_t, RouteObject>& objects) override;
	virtual void onStreamData(const std::vector<StreamPacket>& chunks, const RouteObject& object, const std::vector<uint8_t>& decryptedMP4, uint64_t& baseDts, uint32_t transportObjectId) override;
	bool writePat();

	std::unordered_map<uint16_t, uint8_t> mapCC;
	OutputCallback outputCallback;
	ts::DuckContext duck;
};