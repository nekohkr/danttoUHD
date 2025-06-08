#pragma once
#include <vector>
#include <functional>
extern "C" {
#include <libavutil/error.h>
}
struct MemoryReader {
    const std::vector<uint8_t>& buffer;
    size_t pos;

    MemoryReader(const std::vector<uint8_t>& buf) : buffer(buf), pos(0) {}

    static int read_packet(void* opaque, uint8_t* buf, int buf_size) {
        auto* reader = static_cast<MemoryReader*>(opaque);
        size_t remain = reader->buffer.size() - reader->pos;
        size_t to_read = std::min(remain, static_cast<size_t>(buf_size));

        if (to_read == 0) return AVERROR_EOF;

        memcpy(buf, reader->buffer.data() + reader->pos, to_read);
        reader->pos += to_read;

        return static_cast<int>(to_read);
    }
};

struct MemoryWriter {
    std::vector<uint8_t>& buffer;

    MemoryWriter(std::vector<uint8_t>& buf) : buffer(buf) {}

    static int write_packet(void* opaque, const uint8_t* buf, int buf_size) {
        auto* writer = static_cast<MemoryWriter*>(opaque);
        writer->buffer.insert(writer->buffer.end(), buf, buf + buf_size);
        return buf_size;
    }
};


int ac3Encode(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);