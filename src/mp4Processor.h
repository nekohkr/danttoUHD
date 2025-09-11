#pragma once
#include <map>
#include <array>
#include <vector>
#include <cstdint>
#include <list>
#include "streamPacket.h"

class AP4_Atom;
class AP4_TrunAtom;
class AP4_ContainerAtom;
class AP4_TfdtAtom;
class AP4_TfhdAtom;
class AlcStream;

namespace atsc3 {

struct MP4CodecConfig {
    std::vector<uint8_t> prefixNalUnits;
    uint32_t timescale{ 0 };
    uint8_t nalUnitLengthSize{ 0 };
};

class MP4ConfigParser {
public:
    static bool parse(const std::vector<uint8_t>& input, struct MP4CodecConfig& config);
};

class MP4Processor {
public:
    bool process(const std::vector<uint8_t>& data, std::vector<StreamPacket>& packets);

private:
    bool ProcessMdat(AP4_Atom* trun);
    bool ProcessMoof(AP4_ContainerAtom* trun);
    bool ProcessPssh(AP4_Atom* trun);
    void ProcessSenc(AP4_Atom* trun);
    void ProcessTrun(AP4_TrunAtom* trun);
    void ProcessTfdt(AP4_TfdtAtom* tfdt);
    void ProcessTfhd(AP4_TfhdAtom* tfhd);
    void clear();

    std::array<uint8_t, 16> currentKid;
    std::vector<uint32_t> vecSampleSize;
    std::vector<uint32_t> vecSampleCompositionTimeOffset;
    std::list<std::pair<std::array<uint8_t, 16>, std::array<uint8_t, 16>>> keyCache;
    std::vector<std::array<uint8_t, 16>> vecIv;
    std::vector<uint8_t>* g_output;
    AP4_TrunAtom* g_trun;
    uint64_t baseDts{ 0 };
    uint32_t baseSampleDuration{ 0 };
    std::vector<struct StreamPacket> packets;

};

}