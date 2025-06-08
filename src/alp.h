#pragma once
#include <cstdint>
#include "stream.h"

namespace ATSC3 {

enum class UnpackResult {
	Success = 0,
	NotEnoughData = 1,
	InvalidData = 2,
	Error = 3
};

class ALP {
public:
	UnpackResult unpack(Common::ReadStream& stream);

	uint8_t packetType{ 0 };
	bool payloadConfiguration{ false };
	bool headerMode{ false };
	uint16_t length{ 0 };
	bool segmentationConcatenation;
	std::vector<uint8_t> payload;


};

}