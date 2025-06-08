#include "udp.h"
#include "stream.h"

bool UDPHeader::unpack(Common::ReadStream& stream)
{
    try {
        srcPort = stream.getBe16U();
        dstPort = stream.getBe16U();
        length = stream.getBe16U();
        checksum = stream.getBe16U();
    }
    catch (const std::out_of_range&) {
        return false;
    }

    return true;
}
