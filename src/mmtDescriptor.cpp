#include "mmtDescriptor.h"

namespace atsc3 {

namespace {

#define 	NTP_OFFSET   2208988800ULL
#define 	NTP_OFFSET_US   (NTP_OFFSET * 1000000ULL)

uint64_t ff_parse_ntp_time2(uint64_t ntp_ts)
{
    uint64_t sec = ntp_ts >> 32;
    uint64_t frac_part = ntp_ts & 0xFFFFFFFFULL;
    uint64_t usec = (frac_part * 1000000) / 0xFFFFFFFFULL;

    return (sec * 1000000) + usec;
}

} // anonymous namespace

bool MmtMpuTimestampDescriptor::unpack(Common::ReadStream& stream) {
    uint16_t descriptorTag = stream.getBe16U();
    if (descriptorTag != kDescriptorTag) {
        return false;
    }

    descriptorLength = stream.get8U();
    while (stream.leftBytes() > 0) {
        struct Entry entry;
        entry.mpuSequenceNumber = stream.getBe32U();
        entry.mpuPresentationTime = ff_parse_ntp_time2(stream.getBe64U()) - NTP_OFFSET_US;
        entries.push_back(entry);
    }

    return true;
}

bool MmtDescriptors::unpack(Common::ReadStream& stream) {
    while (stream.leftBytes()) {
        uint16_t descriptorTag = stream.peekBe16U();
        switch (descriptorTag) {
        case MmtMpuTimestampDescriptor::kDescriptorTag:
        {
            MmtMpuTimestampDescriptor descriptor;
            if (!descriptor.unpack(stream)) {
                return false;
            }
            descriptors.push_back(std::make_unique<MmtMpuTimestampDescriptor>(descriptor));
            break;
        }
        default:
            return false;
        }
    }

    return true;
}

}