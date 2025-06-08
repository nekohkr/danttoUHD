#pragma once
#include <cstdint>

namespace Common {
class ReadStream;
}

class UDPHeader {
public:
	bool unpack(Common::ReadStream& stream);

public:
	uint16_t srcPort;
	uint16_t dstPort;
	uint16_t length;
	uint16_t checksum;
};