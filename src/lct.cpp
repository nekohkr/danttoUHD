#include "lct.h"
#include "stream.h"

namespace ATSC3 {

bool LCT::unpack(Common::ReadStream& stream)
{
	uint8_t uint8 = stream.get8U();
	version = (uint8 & 0b11110000) >> 4;
	congestionControlFlag = (uint8 & 0b00001100) >> 2;
	protocolSpecificIndication = uint8 & 0b00000011;

	uint8 = stream.get8U();
	transportSessionIdentifierFlag = (uint8 & 0b10000000) >> 7 != 0;
	transportObjectIdentifierFlag = (uint8 & 0b01100000) >> 5;
	closeSessionFlag = (uint8 & 0b00000010) >> 1 != 0;
	closeObjectFlag = uint8 & 0b00000001;

	headerLength = stream.get8U() * 4;
	codepoint = stream.get8U();

	congestionControlInformation = stream.getBe32U();
	transportSessionId = stream.getBe32U();
	transportObjectId = stream.getBe32U();

	int lastHeaderLength = headerLength - (1 + 1 + 1 + 1 + 4 + 4 + 4);
	stream.skip(lastHeaderLength);

	return true;
}

}