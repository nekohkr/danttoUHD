#pragma once
#include <functional>
#include <iomanip>

#include <ilo/memory.h>

#include <mmtisobmff/helper/commonhelpertools.h>
#include <mmtisobmff/helper/printhelpertools.h>
#include <mmtisobmff/reader/trackreader.h>
#include <mmtisobmff/writer/trackwriter.h>

// project includes
#include <mpeghdecoder.h>
#include <sys/cmdl_parser.h>

#include "wav_file2.h"

using namespace mmt::isobmff;

/********************* decoder configuration structure **********************/
typedef struct {
    MPEGH_DECODER_PARAMETER param;
    const char* desc;
    const char* swText;
    const TEXTCHAR* sw;  // Command line parameter switch for cmdl parsing
} PARAMETER_ASSIGNMENT_TAB;

static uint64_t calcTimestampNs(uint32_t pts, uint32_t timescale) {
    return (uint64_t)((double)pts * (double)1e9 / (double)timescale + 0.5);
}

// I/O buffers
#define IN_BUF_SIZE (65536) /*!< Size of decoder input buffer in bytes. */
#define MAX_RENDERED_CHANNELS (24)
#define MAX_RENDERED_FRAME_SIZE (3072)


static constexpr int32_t defaultCicpSetup = 6;


class MpeghDecoder {
private:
    std::string m_wavFilename;
    bool fileOpen;
    WAV2 m_wavFile;
    CIsobmffReader m_reader;
    HANDLE_MPEGH_DECODER_CONTEXT m_decoder;

public:
    MpeghDecoder(const std::shared_ptr<const ilo::ByteBuffer> input, int32_t cicpSetup)
        :
        m_reader(ilo::make_unique<CIsobmffMemoryInput>(input)) {
        m_decoder = mpeghdecoder_init(cicpSetup);
        if (m_decoder == nullptr) {
            throw std::runtime_error("Error: Unable to create MPEG-H decoder");
        }
    }

    ~MpeghDecoder() {
        if (m_decoder != nullptr) {
            mpeghdecoder_destroy(m_decoder);
        }
    }


    std::vector<uint8_t> process(int32_t startSample, int32_t stopSample, int32_t seekFromSample,
        int32_t seekToSample) {
        uint32_t frameSize = 0;       // Current audio frame size
        uint32_t sampleRate = 0;      // Current samplerate
        int32_t numChannels = -1;     // Current amount of output channels
        int32_t outputLoudness = -2;  // Audio output loudness

        uint32_t sampleCounter = 0;  // mp4 sample counter
        uint32_t frameCounter = 0;   // output frame counter

        int32_t outData[MAX_RENDERED_CHANNELS * MAX_RENDERED_FRAME_SIZE] = { 0 };

        // Only the first MPEG-H track will be processed. Further MPEG-H tracks will be skipped!
        bool mpeghTrackAlreadyProcessed = false;

        // Getting some information about the available tracks

        for (const auto& trackInfo : m_reader.trackInfos()) {
            if (trackInfo.codec != Codec::mpegh_mhm && trackInfo.codec != Codec::mpegh_mha) {
                continue;
            }

            if (mpeghTrackAlreadyProcessed) {
                continue;
            }

            // Create a generic track reader for track number i
            std::unique_ptr<CMpeghTrackReader> mpeghTrackReader =
                m_reader.trackByIndex<CMpeghTrackReader>(trackInfo.trackIndex);

            if (mpeghTrackReader == nullptr) {
                continue;
            }

            std::unique_ptr<config::CMhaDecoderConfigRecord> mhaDcr = nullptr;
            if (trackInfo.codec == Codec::mpegh_mha) {
                mhaDcr = mpeghTrackReader->mhaDecoderConfigRecord();
                if (mhaDcr == nullptr) {
                    continue;
                }
                if (mhaDcr->mpegh3daConfig().empty()) {
                    continue;
                }

                MPEGH_DECODER_ERROR err = mpeghdecoder_setMhaConfig(
                    m_decoder, mhaDcr->mpegh3daConfig().data(), mhaDcr->mpegh3daConfig().size());
                if (err != MPEGH_DEC_OK) {
                    throw std::runtime_error("Error: Unable to set mpeghDecoder MHA configuration");
                }
            }

            // check if enough samples are available to start at requested sample
            if (startSample >= 0 && static_cast<uint32_t>(startSample) >= trackInfo.sampleCount) {
                throw std::runtime_error(
                    "[" + std::to_string(sampleCounter) + "] Error: Too few ISOBMFF/MP4 Samples (" +
                    std::to_string(trackInfo.sampleCount) +
                    ") in track for starting at ISOBMFF/MP4 sample " + std::to_string(startSample));
            }

            // check if enough samples are available to seek to requested sample
            if (seekToSample >= 0 && static_cast<uint32_t>(seekToSample) >= trackInfo.sampleCount) {
                throw std::runtime_error(
                    "[" + std::to_string(sampleCounter) + "] Error: Too few ISOBMFF/MP4 Samples (" +
                    std::to_string(trackInfo.sampleCount) +
                    ") in track for seeking to ISOBMFF/MP4 sample " + std::to_string(seekToSample));
            }

            // Preallocate the sample with max sample size to avoid reallocation of memory.
            // Sample can be re-used for each nextSample call.
            CSample sample{ trackInfo.maxSampleSize };

            // Get samples starting with the provided start sample
            sampleCounter = startSample;
            SSampleExtraInfo sampleInfo = mpeghTrackReader->sampleByIndex(sampleCounter, sample);
            SSeekConfig seekConfig(
                CTimeDuration(sampleInfo.timestamp.timescale(), sampleInfo.timestamp.ptsValue()),
                ESampleSeekMode::nearestSyncSample);
            sampleInfo = mpeghTrackReader->sampleByTimestamp(seekConfig, sample);

            bool seekPerformed = false;
            while (!sample.empty() && sampleCounter <= static_cast<uint32_t>(stopSample)) {
                MPEGH_DECODER_ERROR err = MPEGH_DEC_OK;
                // Feed the sample data to the decoder.
                err = mpeghdecoder_processTimescale(m_decoder, sample.rawData.data(), sample.rawData.size(),
                    sampleInfo.timestamp.ptsValue(),
                    sampleInfo.timestamp.timescale());
                if (err != MPEGH_DEC_OK) {
                    throw std::runtime_error("[" + std::to_string(sampleCounter) +
                        "] Error: Unable to process data");
                }

                sampleCounter++;

                if (!seekPerformed && sampleCounter == static_cast<uint32_t>(seekFromSample)) {
                    err = mpeghdecoder_flush(m_decoder);
                    if (err != MPEGH_DEC_OK) {
                        throw std::runtime_error("[" + std::to_string(sampleCounter) +
                            "] Error: Unable to flush decoder");
                    }
                    seekPerformed = true;
                    sampleCounter = seekToSample;
                    sampleInfo = mpeghTrackReader->sampleByIndex(sampleCounter, sample);
                    seekConfig = SSeekConfig(
                        CTimeDuration(sampleInfo.timestamp.timescale(), sampleInfo.timestamp.ptsValue()),
                        ESampleSeekMode::nearestSyncSample);
                    sampleInfo = mpeghTrackReader->sampleByTimestamp(seekConfig, sample);
                }
                else {
                    // Get the next sample.
                    sampleInfo = mpeghTrackReader->nextSample(sample);
                }

                // Check if EOF or the provided stop sample is reached.
                if (!sample.empty() && sampleCounter <= static_cast<uint32_t>(stopSample)) {
                    // Stop sample is not reached.
                }
                else {
                    // Stop sample is reached. -> Flush the remaining output frames from the decoder.
                    err = mpeghdecoder_flushAndGet(m_decoder);
                    if (err != MPEGH_DEC_OK) {
                        throw std::runtime_error("Error: Unable to flush data");
                    }
                }

                // Obtain decoded audio frames.
                MPEGH_DECODER_ERROR status = MPEGH_DEC_OK;
                MPEGH_DECODER_OUTPUT_INFO outInfo;
                while (status == MPEGH_DEC_OK) {
                    status = mpeghdecoder_getSamples(m_decoder, outData, sizeof(outData) / sizeof(int32_t),
                        &outInfo);

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
            }

            mpeghTrackAlreadyProcessed = true;
        }

        if (fileOpen) {
            WAV_OutputFlush2(m_wavFile);
            return std::move(m_wavFile.buffer);
        }
        return {};
    }

};
