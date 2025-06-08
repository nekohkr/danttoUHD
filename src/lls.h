#pragma once
#include <cstdint>
#include "stream.h"

namespace ATSC3 {

class LLS {
public:
	bool unpack(Common::ReadStream& stream);

public:
	uint8_t tableId;
	uint8_t groupId;
	uint8_t groupCount;
	uint8_t tableVersion;

};

}