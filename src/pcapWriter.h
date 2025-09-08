#pragma once
#include <cstdint>
#include <vector>
#include <fstream>
#include <chrono>
#include <cstring>

#pragma pack(push,1)
struct PcapGlobalHeader {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
};

struct PcapPacketHeader {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};
#pragma pack(pop)

class PcapWriter {
    std::ofstream ofs;
public:
    bool open(const char* path, uint32_t snaplen = 65535, uint32_t linktype = 101) {
        ofs.open(path, std::ios::binary);
        if (!ofs) return false;
        PcapGlobalHeader gh;
        gh.magic_number = 0xa1b2c3d4u;
        gh.version_major = 2;
        gh.version_minor = 4;
        gh.thiszone = 0;
        gh.sigfigs = 0;
        gh.snaplen = snaplen;
        gh.network = linktype;
        ofs.write(reinterpret_cast<const char*>(&gh), sizeof(gh));
        return bool(ofs);
    }

    bool writePacket(const std::vector<uint8_t>& data, std::chrono::system_clock::time_point tp = std::chrono::system_clock::now()) {
        if (!ofs) return false;
        using namespace std::chrono;
        auto s = time_point_cast<seconds>(tp);
        auto us = duration_cast<microseconds>(tp - s).count();
        PcapPacketHeader ph;
        ph.ts_sec = static_cast<uint32_t>(s.time_since_epoch().count());
        ph.ts_usec = static_cast<uint32_t>(us);
        ph.incl_len = static_cast<uint32_t>(data.size());
        ph.orig_len = static_cast<uint32_t>(data.size());
        ofs.write(reinterpret_cast<const char*>(&ph), sizeof(ph));
        if (!ofs) return false;
        if (!data.empty()) ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
        return bool(ofs);
    }

    void close() {
        if (ofs) ofs.close();
    }

    ~PcapWriter() { close(); }
};