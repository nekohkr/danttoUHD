#include "lls.h"
#include "stream.h"
#include  <zlib.h>
#include "decompress.h"

namespace ATSC3 {

bool LLS::unpack(Common::ReadStream& stream)
{
    tableId = stream.get8U();
    groupId = stream.get8U();
    groupCount = stream.get8U();
    tableVersion = stream.get8U();

    std::vector<uint8_t> compressed;
    compressed.resize(stream.leftBytes());
    stream.read(compressed.data(), compressed.size());

    std::optional<std::string> decompress = gzipInflate(compressed);
    if (!decompress) {
        return false;
    }

    payload = decompress.value();
    return true;
}

}