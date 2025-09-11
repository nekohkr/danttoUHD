#pragma once
#include <functional>
#include <iomanip>
#include <ilo/memory.h>
#include <mmtisobmff/helper/printhelpertools.h>
#include <mmtisobmff/reader/trackreader.h>
#include <mmtisobmff/writer/trackwriter.h>
#include <mpeghdecoder.h>
#include <mmtisobmff/reader/reader.h>
#include <mmtisobmff/logging.h>
#include <mpeghUIManager.h>
#include "wav_file2.h"

using namespace mmt::isobmff;

#define MAX_RENDERED_CHANNELS (24)
#define MAX_RENDERED_FRAME_SIZE (3072)


#define PERSISTENCE_BUFFER_SIZE 2048
#define MAX_MPEGH_FRAME_SIZE 65536
static constexpr int32_t defaultCicpSetup = 6;

struct MpeghDecoderResult {
    uint32_t sampleCount;
    std::vector<uint8_t> wav;
};

class MpeghDecoder {
private:
    std::string m_wavFilename;
    bool fileOpen;
    WAV2 m_wavFile;
    HANDLE_MPEGH_DECODER_CONTEXT m_decoder;
    uint64_t pts{ 0 };
    HANDLE_MPEGH_UI_MANAGER m_uiManager;
    std::vector<uint16_t> m_persistenceMemory;

public:
    MpeghDecoder() {
        disableLogging();

        m_decoder = mpeghdecoder_init(6);
        if (m_decoder == nullptr) {
            throw std::runtime_error("Error: Unable to create MPEG-H decoder");
        }

        m_uiManager = mpegh_UI_Manager_Open();
        if (m_uiManager == nullptr) {
            throw std::runtime_error("Error: Unable to create MPEG-H UI manager");
        }

        m_persistenceMemory.resize(PERSISTENCE_BUFFER_SIZE / sizeof(uint16_t));
        mpegh_UI_SetPersistenceMemory(m_uiManager, m_persistenceMemory.data(), PERSISTENCE_BUFFER_SIZE);
    }

    ~MpeghDecoder() {
        if (m_decoder != nullptr) {
            mpeghdecoder_destroy(m_decoder);
        }
    }

    struct MpeghDecoderResult feed(const std::vector<struct StreamPacket>& packets, uint64_t sampleDuration, uint32_t timescale) {
        struct MpeghDecoderResult result;
        uint32_t sampleCounter = 0;

        for (const auto& packet : packets) {

            CSample sample = CSample{ MAX_MPEGH_FRAME_SIZE };
            sample.duration = sampleDuration;
            sample.rawData = packet.data;

            sample.fragmentNumber = 0;
            processSingleSample(sample);
            decodeSingleSample(timescale, sample);

            sampleCounter++;

            result.sampleCount = sampleCounter;
        }

        WAV_OutputFlush2(m_wavFile);
        result.wav = std::move(m_wavFile.buffer);
        return result;
    }

    void processSingleSample(mmt::isobmff::CSample& sample) {
        uint32_t mhasLength = static_cast<uint32_t>(sample.rawData.size());
        MPEGH_UI_ERROR feedErr = mpegh_UI_FeedMHAS(m_uiManager, sample.rawData.data(), mhasLength);

        if (feedErr == MPEGH_UI_OK) {
            sample.rawData.resize(MAX_MPEGH_FRAME_SIZE);
            uint32_t newMhasLength = mhasLength;
            // Update the MPEG-H frame
            MPEGH_UI_ERROR err = mpegh_UI_UpdateMHAS(m_uiManager, sample.rawData.data(),
                MAX_MPEGH_FRAME_SIZE, &newMhasLength);
            if (err == MPEGH_UI_NOT_ALLOWED) {
                sample.rawData.resize(mhasLength);
            }
            else if (err != MPEGH_UI_OK) {
                sample.rawData.resize(mhasLength);
            }
            else {
                sample.rawData.resize(newMhasLength);
            }
        }
    }

    std::vector<uint8_t> decodeSingleSample(uint32_t timescale, CSample sample) {
        uint32_t frameSize = 0;
        uint32_t sampleRate = 0;
        int32_t numChannels = -1;
        int32_t outputLoudness = -2;
        uint32_t sampleCounter = 0;
        uint32_t frameCounter = 0;
        int32_t outData[MAX_RENDERED_CHANNELS * MAX_RENDERED_FRAME_SIZE] = { 0 };
        MPEGH_DECODER_ERROR err = MPEGH_DEC_OK;

        err = mpeghdecoder_processTimescale(m_decoder, sample.rawData.data(), static_cast<uint32_t>(sample.rawData.size()),
            pts, timescale);
        pts += sample.duration;

        MPEGH_DECODER_ERROR status = MPEGH_DEC_OK;
        MPEGH_DECODER_OUTPUT_INFO outInfo;
        while (status == MPEGH_DEC_OK) {
            status = mpeghdecoder_getSamples(m_decoder, outData, sizeof(outData) / sizeof(int32_t), &outInfo);

            if (status != MPEGH_DEC_OK && status != MPEGH_DEC_FEED_DATA) {
                throw std::runtime_error("[" + std::to_string(frameCounter) +
                    "] Error: Unable to obtain output");
            }
            else if (status == MPEGH_DEC_OK) {
                if (outInfo.sampleRate != 48000 && outInfo.sampleRate != 44100) {
                    throw std::runtime_error("Error: Unsupported sampling rate");
                }
                if (outInfo.numChannels <= 0) {
                    throw std::runtime_error("Error: Unsupported number of channels");
                }
                if (sampleRate != 0 && sampleRate != static_cast<uint32_t>(outInfo.sampleRate)) {
                    throw std::runtime_error("Error: Unsupported change of sampling rate");
                }
                if (numChannels != -1 && numChannels != outInfo.numChannels) {
                    throw std::runtime_error("Error: Unsupported change of number of channels");
                }

                frameSize = outInfo.numSamplesPerChannel;
                sampleRate = outInfo.sampleRate;
                numChannels = outInfo.numChannels;

                if (outputLoudness != outInfo.loudness) {
                    outputLoudness = outInfo.loudness;
                }
                if (!fileOpen && sampleRate && numChannels) {
                    if (WAV_OutputOpen2(m_wavFile, "", sampleRate, numChannels,
                        16)) {
                    }
                    fileOpen = true;
                }

                if (fileOpen && WAV_OutputWrite2(m_wavFile, outData, numChannels * frameSize,
                    32, 32)) {
                    throw std::runtime_error("[" + std::to_string(frameCounter) +
                        "] Error: Unable to write to output file " + m_wavFilename);
                }

                frameCounter++;
            }
        }
        return {};
    }

};
