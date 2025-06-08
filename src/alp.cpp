#include "alp.h"

namespace ATSC3 {

UnpackResult ALP::unpack(Common::ReadStream& s)
{
	uint16_t uint16 = s.getBe16U();
	packetType = (uint16 & 0b1110000000000000) >> 13;
	payloadConfiguration = static_cast<bool>((uint16 & 0b0001000000000000) >> 12);


	if (payloadConfiguration == 0) {
		headerMode = (uint16 & 0b0000100000000000) >> 11;
		length = uint16 & 0b0000011111111111;

		if (headerMode == 1) {
			uint8_t uint8 = s.get8U();
			uint8_t headerLengthMSB = (uint8 & 0b11111000) >> 3;
			uint8_t headerReserved = (uint8 & 0b00000100) >> 2;
			uint8_t sif = (uint8 & 0b00000010) >> 1;
			uint8_t hef = uint8 & 0b00000001;

			if (sif == 1) {
				uint8_t subStreamIdentification = s.get8U();
			}
		}
	}
	else {
		uint8_t segmentationConcatenation = (uint16 & 0b0000100000000000) >> 11;
		length = uint16 & 0b0000011111111111;

		if (segmentationConcatenation == 0) {
			uint8_t uint8 = s.get8U();
			uint8_t segmentSequenceNumber = (uint8 & 0b11111000) >> 3;
			uint8_t lastSegmentIndicator = (uint8 & 0b00000100) >> 2;
			uint8_t sif = (uint8 & 0b00000010) >> 1;
			uint8_t hef = uint8 & 0b00000001;

			if (sif == 1) {
				uint8_t subStreamIdentification = s.get8U();
			}

			// todo
		}
		else {
			// todo
		}
	}

	if (s.leftBytes() < length) {
		return UnpackResult::NotEnoughData;
	}

	payload.resize(length);
	s.read(payload.data(), length);
	return UnpackResult::Success;
}

}
