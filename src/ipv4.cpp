#include "ipv4.h"
#include "stream.h"

bool IPv4Header::unpack(Common::ReadStream& stream)
{
    try {
        uint16_t uint8 = stream.get8U();
        version = (uint8 & 0b11110000) >> 4;
        ihl = uint8 & 0b00001111;

        if (version != 4) {
            return false;
        }

        tos = stream.get8U();
        length = stream.getBe16U();
        id = stream.getBe16U();

        uint16_t uint16 = stream.getBe16U();
        flags = (uint16 & 0b1110000000000000) >> 14;
        fragmentOffset = uint16 & 0b0001111111111111;

        ttl = stream.get8U();
        protocol = stream.get8U();
        headerChecksum = stream.getBe16U();

        srcIpAddr = stream.get32U();
        dstIpAddr = stream.get32U();

    }
    catch (const std::out_of_range&) {
        return false;
    }

    return true;
}
