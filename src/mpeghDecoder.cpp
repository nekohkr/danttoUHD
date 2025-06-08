
#include <functional>
#include <iomanip>

#include <ilo/memory.h>
#include <mmtisobmff/types.h>
#include <mmtisobmff/logging.h>
#include <mmtisobmff/reader/input.h>
#include <mmtisobmff/reader/reader.h>
#include <mmtisobmff/helper/printhelpertools.h>
#include <mmtisobmff/reader/trackreader.h>

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


class CProcessor {
private:
    std::string m_wavFilename;
    HANDLE_WAV2 m_wavFile;
    CIsobmffReader m_reader;
    HANDLE_MPEGH_DECODER_CONTEXT m_decoder;

public:
    CProcessor(const std::shared_ptr<const ilo::ByteBuffer> input, int32_t cicpSetup)
        :
        m_wavFile(nullptr),
        m_reader(ilo::make_unique<CIsobmffMemoryInput>(input)) {
        m_decoder = mpeghdecoder_init(cicpSetup);
        if (m_decoder == nullptr) {
            throw std::runtime_error("Error: Unable to create MPEG-H decoder");
        }
    }

    ~CProcessor() {
        if (m_decoder != nullptr) {
            mpeghdecoder_destroy(m_decoder);
        }

        if (m_wavFile != nullptr) {
            WAV_OutputClose2(&m_wavFile);
        }
    }

    std::vector<uint8_t> process(int32_t startSample, int32_t stopSample, int32_t seekFromSample, int32_t seekToSample) {
        uint32_t frameSize = 0;       // Current audio frame size
        uint32_t sampleRate = 0;      // Current samplerate
        int32_t numChannels = -1;     // Current amount of output channels
        int32_t outputLoudness = -2;  // Audio output loudness

        uint32_t sampleCounter = 0;  // mp4 sample counter
        uint32_t frameCounter = 0;   // output frame counter
        uint64_t timestamp = 0;      // current sample timestamp in nanoseconds

        int32_t outData[MAX_RENDERED_CHANNELS * MAX_RENDERED_FRAME_SIZE] = { 0 };

        // Only the first MPEG-H track will be processed. Further MPEG-H tracks will be skipped!
        bool mpeghTrackAlreadyProcessed = false;

        std::vector<uint8_t> chunks;

        for (const auto& trackInfo : m_reader.trackInfos()) {
            if (trackInfo.codec != Codec::mpegh_mhm && trackInfo.codec != Codec::mpegh_mha) {
                std::cout << "Skipping unsupported codec: " << ilo::toString(trackInfo.codingName)
                    << std::endl;
                std::cout << std::endl;
                break;
            }

            if (mpeghTrackAlreadyProcessed) {
                std::cout << "Skipping further mhm1 and mha1 tracks!" << std::endl;
                std::cout << std::endl;
                break;
            }

            std::unique_ptr<CMpeghTrackReader> mpeghTrackReader =
                m_reader.trackByIndex<CMpeghTrackReader>(trackInfo.trackIndex);

            if (mpeghTrackReader == nullptr) {
                std::cout << "Error: Track reader could not be created! Skipping track!" << std::endl;
                break;
            }

            std::unique_ptr<config::CMhaDecoderConfigRecord> mhaDcr = nullptr;
            if (trackInfo.codec == Codec::mpegh_mha) {
                mhaDcr = mpeghTrackReader->mhaDecoderConfigRecord();
                if (mhaDcr == nullptr) {
                    std::cout << "Warning: No MHA config available! Skipping track!" << std::endl;
                    break;
                }
                if (mhaDcr->mpegh3daConfig().empty()) {
                    std::cout << "Warning: MHA config is empty! Skipping track!" << std::endl;
                    break;
                }

                MPEGH_DECODER_ERROR err = mpeghdecoder_setMhaConfig(
                    m_decoder, mhaDcr->mpegh3daConfig().data(), mhaDcr->mpegh3daConfig().size());
                if (err != MPEGH_DEC_OK) {
                    throw std::runtime_error("Error: Unable to set mpeghDecoder MHA configuration");
                }

            }

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

            bool seekPerformed = false;
            // Calculate the timestamp in nano seconds
            timestamp =
                calcTimestampNs(sampleInfo.timestamp.ptsValue(), sampleInfo.timestamp.timescale());
            while (!sample.empty() && sampleCounter <= static_cast<uint32_t>(stopSample)) {
                MPEGH_DECODER_ERROR err = MPEGH_DEC_OK;
                // Feed the sample data to the decoder.
                err = mpeghdecoder_process(m_decoder, sample.rawData.data(), sample.rawData.size(),
                    timestamp);
                if (err != MPEGH_DEC_OK) {
                    throw std::runtime_error("[" + std::to_string(sampleCounter) +
                        "] Error: Unable to process data");
                }

                sampleCounter++;
                //std::cout << "ISOBMFF/MP4 Samples processed: " << sampleCounter << "\r" << std::flush;

                if (!seekPerformed && sampleCounter == static_cast<uint32_t>(seekFromSample)) {
                    std::cout << "Performing seek from ISOBMFF/MP4 Sample " << seekFromSample
                        << " to ISOBMFF/MP4 Sample " << seekToSample << std::endl;
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
                }
                else {
                    // Get the next sample.
                    sampleInfo = mpeghTrackReader->nextSample(sample);
                }

                // Check if EOF or the provided stop sample is reached.
                if (!sample.empty() && sampleCounter <= static_cast<uint32_t>(stopSample)) {
                    // Stop sample is not reached; get the sample's timestamp in nano seconds
                    timestamp =
                        calcTimestampNs(sampleInfo.timestamp.ptsValue(), sampleInfo.timestamp.timescale());
                }
                else {
                    // Stop sample is reached. -> Flush the remaining output frames from the decoder.
                    //std::cout << std::endl;
                    //std::cout << "Flushing the decoder!" << std::endl;
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

                        if (!m_wavFile && sampleRate && numChannels) {
                            if (WAV_OutputOpen2(&m_wavFile, "", sampleRate, numChannels,
                                24)) {
                                throw std::runtime_error("[" + std::to_string(frameCounter) +
                                    "] Error: Unable to create output file " +
                                    m_wavFilename);
                            }
                        }

                        if (m_wavFile && WAV_OutputWrite2(m_wavFile, outData, numChannels * frameSize,
                            SAMPLE_BITS, SAMPLE_BITS)) {
                            throw std::runtime_error("[" + std::to_string(frameCounter) +
                                "] Error: Unable to write to output file " + m_wavFilename);
                        }
                        frameCounter++;
                    }
                }
            }

            if (m_wavFile) {
                WAV_OutputFlush2(&m_wavFile);
                chunks = *m_wavFile->buffer;
                WAV_OutputClose2(&m_wavFile);
                return chunks;
            }

            return std::vector<uint8_t>{};
        }

        return std::vector<uint8_t>{};
    }
};

int mpeghDecode(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    disableLogging();

    int32_t stopSample = std::numeric_limits<int32_t>::max();
    int32_t startSample = 0;
    int32_t seekFromSample = -1;
    int32_t seekToSample = -1;
    int32_t cicpSetup = defaultCicpSetup;

    try {
        CProcessor processor(std::make_shared<std::vector<uint8_t>>(input), cicpSetup);
        std::vector<uint8_t> output2 = processor.process(startSample, stopSample, seekFromSample, seekToSample);
        output = output2;

    }
    catch (const std::exception& e) {
        std::cout << std::endl << "Error: " << e.what() << std::endl << std::endl;
        return 0;
    }
    catch (...) {
        std::cout << std::endl
            << "Error: An unknown error happened. The program will exit now." << std::endl
            << std::endl;
        return 0;
    }
    return 1;
}
