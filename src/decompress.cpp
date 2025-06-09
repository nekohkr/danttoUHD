#include "decompress.h"
#include <zlib.h>

std::optional<std::string> gzipInflate(const std::vector<uint8_t>& input) {
    z_stream strm = {};
    strm.next_in = const_cast<uint8_t*>(input.data());
    strm.avail_in = input.size();

    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
        return {};
    }

    std::string output;
    constexpr size_t chunkSize = 4096;
    uint8_t outBuffer[chunkSize];
    int ret;

    do {
        strm.next_out = outBuffer;
        strm.avail_out = chunkSize;

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            return {};
        }

        size_t have = chunkSize - strm.avail_out;
        output.insert(output.end(), outBuffer, outBuffer + have);

    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    return output;
}
