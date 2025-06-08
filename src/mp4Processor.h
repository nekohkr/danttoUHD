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

class MP4ConfigParser {
public:
    struct MP4Config {
        std::vector<uint8_t> configNalUnits;
        uint32_t timescale{ 0 };
    };

    static bool parse(const std::vector<uint8_t>& input, MP4Config& config);

};

class MP4Processor {
public:
    bool process(const std::vector<uint8_t>& data, std::vector<StreamPacket>& packets, std::vector<uint8_t>& outputMp4, uint64_t& baseDts_);
    

private:
    bool ProcessMdat(AP4_Atom* trun, std::vector<uint8_t>& outputMp4);
    bool ProcessMoof(AP4_ContainerAtom* trun);
    bool ProcessPssh(AP4_Atom* trun);
    void ProcessSenc(AP4_Atom* trun);
    void ProcessTrun(AP4_TrunAtom* trun);
    void ProcessTfdt(AP4_TfdtAtom* tfdt);
    bool ProcessTfhd(AP4_TfhdAtom* tfhd);
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
    std::vector<StreamPacket> packets;

};