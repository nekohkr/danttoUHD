#pragma once

namespace ATSC3 {

enum class AlpPacketType {
	IPv4 = 0x0,
	RESERVED_001 = 0x1,
	COMPRESSED_IP = 0x2,
	RESERVED_011 = 0x3,
	LINK_LAYER_SIGNALLING = 0x4,
	RESERVED_101 = 0x5,
	PACKET_TYPE_EXTENSION = 0x6,
	MPEG2_TS = 0x7
};

}