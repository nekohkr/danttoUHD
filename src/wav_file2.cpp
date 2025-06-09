#include "wav_file2.h"
#include <vector>
#include <stdlib.h>
#include <sys/genericStds.h>

/************** Writer ***********************/
uint32_t FDKfwrite_EL2(const void* ptrf, INT size, uint32_t nmemb, std::vector<uint8_t>& buffer) {
    if (!ptrf || size <= 0 || nmemb == 0) return 0;

    uint32_t totalSize = static_cast<uint32_t>(size) * nmemb;
    size_t oldSize = buffer.size();
    buffer.resize(oldSize + totalSize);

    std::memcpy(buffer.data() + oldSize, ptrf, totalSize);
    return nmemb;
}

/************** Writer ***********************/
uint32_t FDKfwrite_EL_Head(const void* ptrf, INT size, uint32_t nmemb, std::vector<uint8_t>& buffer) {
    buffer.insert(buffer.begin(), (uint8_t*)ptrf, (uint8_t*)ptrf + size * nmemb);
    return size * nmemb;
}

static UINT64 LittleEndian64(UINT64 v) {
    if (IS_LITTLE_ENDIAN())
        return v;
    else
        return (v & 0x00000000000000FFLL) << 56 | (v & 0x000000000000FF00LL) << 40 |
        (v & 0x0000000000FF0000LL) << 24 | (v & 0x00000000FF000000LL) << 8 |
        (v & 0x000000FF00000000LL) >> 8 | (v & 0x0000FF0000000000LL) >> 24 |
        (v & 0x00FF000000000000LL) >> 40 | (v & 0xFF00000000000000LL) >> 56;
}

static uint32_t LittleEndian32(uint32_t v) {
    if (IS_LITTLE_ENDIAN())
        return v;
    else
        return (v & 0x000000FF) << 24 | (v & 0x0000FF00) << 8 | (v & 0x00FF0000) >> 8 |
        (v & 0xFF000000) >> 24;
}

static short LittleEndian16(short v) {
    if (IS_LITTLE_ENDIAN())
        return v;
    else
        return (short)(((v << 8) & 0xFF00) | ((v >> 8) & 0x00FF));
}

static uint16_t Unpack(uint16_t v) {
    if (IS_LITTLE_ENDIAN())
        return v;
    else
        return (short)(((v << 8) & 0xFF00) | ((v >> 8) & 0x00FF));
}

uint32_t WAV_WriteHeader(WAV2& wav) {
    uint32_t size = 0;

    std::vector<uint8_t> header;

    size += (FDKfwrite_EL2(&wav.header.riffType, 1, 4, header)) *
        sizeof(wav.header.riffType[0]);  // RIFF Type
    size += (FDKfwrite_EL2(&wav.header.riffSize, 4, 1, header)) *
        sizeof(wav.header.riffSize);  // RIFF Size
    size += (FDKfwrite_EL2(&wav.header.waveType, 1, 4, header)) *
        sizeof(wav.header.waveType[0]);  // WAVE Type
    size += (FDKfwrite_EL2(&wav.header.ds64Chunk.chunkID, 1, 4, header)) *
        sizeof(wav.header.ds64Chunk.chunkID[0]);  // JUNK/RF64 Type
    size += (FDKfwrite_EL2(&wav.header.ds64Chunk.chunkSize, 4, 1, header)) *
        sizeof(wav.header.ds64Chunk.chunkSize);  // JUNK/RF64 Size
    size += (FDKfwrite_EL2(&wav.header.ds64Chunk.riffSize64, 8, 1, header)) *
        sizeof(wav.header.ds64Chunk.riffSize64);  // Riff Size (64 bit)
    size += (FDKfwrite_EL2(&wav.header.ds64Chunk.dataSize64, 8, 1, header)) *
        sizeof(wav.header.ds64Chunk.dataSize64);  // Data Size (64 bit)
    size += (FDKfwrite_EL2(&wav.header.ds64Chunk.samplesPerCh64, 8, 1, header)) *
        sizeof(wav.header.ds64Chunk.samplesPerCh64);  // Number of Samples
    size += (FDKfwrite_EL2(&wav.header.ds64Chunk.tableLength, 4, 1, header)) *
        sizeof(wav.header.ds64Chunk.tableLength);  // Number of valid entries in array table
    size += (FDKfwrite_EL2(&wav.header.formatType, 1, 4, header)) *
        sizeof(wav.header.formatType[0]);  // format Type
    size += (FDKfwrite_EL2(&wav.header.formatSize, 4, 1, header)) *
        sizeof(wav.header.formatSize);  // format Size
    size += (FDKfwrite_EL2(&wav.header.compressionCode, 2, 1, header)) *
        sizeof(wav.header.compressionCode);  // compression Code
    size += (FDKfwrite_EL2(&wav.header.numChannels, 2, 1, header)) *
        sizeof(wav.header.numChannels);  // numChannels
    size += (FDKfwrite_EL2(&wav.header.sampleRate, 4, 1, header)) *
        sizeof(wav.header.sampleRate);  // sampleRate
    size += (FDKfwrite_EL2(&wav.header.bytesPerSecond, 4, 1, header)) *
        sizeof(wav.header.bytesPerSecond);  // bytesPerSecond
    size += (FDKfwrite_EL2(&wav.header.blockAlign, 2, 1, header)) *
        sizeof(wav.header.blockAlign);  // blockAlign
    size += (FDKfwrite_EL2(&wav.header.bitsPerSample, 2, 1, header)) *
        sizeof(wav.header.bitsPerSample);  // bitsPerSample
    size += (FDKfwrite_EL2(&wav.header.dataType, 1, 4, header)) *
        sizeof(wav.header.dataType[0]);  // dataType
    size += (FDKfwrite_EL2(&wav.header.dataSize, 4, 1, header)) *
        sizeof(wav.header.dataSize);  // dataSize


    wav.buffer.insert(wav.buffer.begin(), header.begin(), header.end());

    return size;
}

/**
 * WAV_OutputOpen
 * \brief Open WAV output/writer handle
 * \param pWav pointer to WAV handle to be returned
 * \param sampleRate desired samplerate of the resulting WAV file
 * \param numChannels desired number of audio channels of the resulting WAV file
 * \param bitsPerSample desired number of bits per audio sample of the resulting WAV file
 *
 * \return value:   0: ok
 *                 -1: error
 */
int WAV_OutputOpen2(WAV2& wav, const char* outputFilename, int sampleRate, int numChannels,
    int bitsPerSample) {
    wav.buffer.reserve(15000);
    uint32_t size = 0;

    if (bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32 &&
        bitsPerSample != NBITS_FLOAT && bitsPerSample != NBITS_DOUBLE) {
        FDKprintfErr("WAV_OutputOpen(): Invalid argument (bitsPerSample).\n");
        return -1;
    }

    FDKstrncpy(wav.header.riffType, "RIFF", 4);
    wav.header.riffSize =
        LittleEndian32(0x7fffffff); /* in case fseek() doesn't work later in WAV_OutputClose() */
    FDKstrncpy(wav.header.waveType, "WAVE", 4);

    /* Create 'JUNK' chunk for possible transition to RF64  */
    FDKstrncpy(wav.header.ds64Chunk.chunkID, "JUNK", 4);
    wav.header.ds64Chunk.chunkSize = LittleEndian32(28);
    wav.header.ds64Chunk.riffSize64 = LittleEndian64(0);
    wav.header.ds64Chunk.dataSize64 = LittleEndian64(0);
    wav.header.ds64Chunk.samplesPerCh64 = LittleEndian64(0);
    wav.header.ds64Chunk.tableLength = LittleEndian32(0);

    FDKstrncpy(wav.header.formatType, "fmt ", 4);
    wav.header.formatSize = LittleEndian32(16);

    wav.header.compressionCode = LittleEndian16(0x01);

    if (bitsPerSample == NBITS_FLOAT) {
        wav.header.compressionCode = LittleEndian16(0x03);
        bitsPerSample = 32;
    }
    if (bitsPerSample == NBITS_DOUBLE) {
        wav.header.compressionCode = LittleEndian16(0x03);
        bitsPerSample = 64;
    }

    wav.header.bitsPerSample = LittleEndian16((short)bitsPerSample);
    wav.header.numChannels = LittleEndian16((short)numChannels);
    wav.header.blockAlign = LittleEndian16((short)(numChannels * (bitsPerSample >> 3)));
    wav.header.sampleRate = LittleEndian32(sampleRate);
    wav.header.bytesPerSecond = LittleEndian32(sampleRate * wav.header.blockAlign);
    FDKstrncpy(wav.header.dataType, "data", 4);
    wav.header.dataSize = LittleEndian32(
        0x7fffffff - WAV_HEADER_SIZE - (sizeof(wav.header.riffType) + sizeof(wav.header.riffSize)));

    wav.header.dataSize = wav.header.riffSize = 0;

    return 0;

}

/**
 * WAV_OutputWrite
 * \brief Write data to WAV file asociated to WAV handle
 *
 * \param wav handle of wave file
 * \param sampleBuffer pointer to audio samples, right justified integer values
 * \param nBufBits size in bits of each audio sample in sampleBuffer
 * \param nSigBits amount of significant bits of each nBufBits in sampleBuffer
 *
 * \return value:    0: ok
 *                  -1: error
 */
int WAV_OutputWrite2(WAV2& wav, void* sampleBuffer, uint32_t numberOfSamples, int nBufBits,
    int nSigBits) {
    char* bptr = (char*)sampleBuffer;
    short* sptr = (short*)sampleBuffer;
    LONG* lptr = (LONG*)sampleBuffer;
    float* fptr = (float*)sampleBuffer;
    double* dptr = (double*)sampleBuffer;
    LONG tmp;

    int bps = Unpack(wav.header.bitsPerSample);
    int wavIEEEfloat = (Unpack(wav.header.compressionCode) == 0x03);
    uint32_t i;

    if ((nBufBits == NBITS_FLOAT) || (nSigBits == NBITS_FLOAT)) {
        nBufBits = 32;
        nSigBits = NBITS_FLOAT;
    }
    if ((nBufBits == NBITS_DOUBLE) || (nSigBits == NBITS_DOUBLE)) {
        nBufBits = 64;
        nSigBits = NBITS_DOUBLE;
    }

    /* Pack samples if required */
    if ((!wavIEEEfloat && bps == nBufBits && bps == nSigBits) ||
        (wavIEEEfloat && bps == nBufBits && nSigBits < 0)) {
        if (FDKfwrite_EL2(sampleBuffer, (bps >> 3), numberOfSamples, wav.buffer) != numberOfSamples) {
            FDKprintfErr("WAV_OutputWrite(): error: unable to write to file %d\n", wav.buffer);
            return -1;
        }
    }
    else if (!wavIEEEfloat) {
        for (i = 0; i < numberOfSamples; i++) {
            int result;
            int shift;

            if ((nSigBits == NBITS_FLOAT) || (nSigBits == NBITS_DOUBLE)) {
                double d;
                double scl = (double)(1UL << (bps - 1));

                if (nSigBits == NBITS_FLOAT)
                    d = *fptr++;
                else
                    d = *dptr++;

                d *= scl;
                if (d >= 0)
                    d += 0.5;
                else
                    d -= 0.5;

                if (d >= scl) d = scl - 1;
                if (d < -scl) d = -scl;

                tmp = (LONG)d;
            }
            else {
                switch (nBufBits) {
                case 8:
                    tmp = *bptr++;
                    break;
                case 16:
                    tmp = *sptr++;
                    break;
                case 32:
                    tmp = *lptr++;
                    break;
                default:
                    return -1;
                }
                /* Adapt sample size */
                shift = bps - nSigBits;

                /* Correct alignment difference between 32 bit data buffer "tmp" and 24 bits to be written.
                 */
                if (!IS_LITTLE_ENDIAN() && bps == 24) {
                    shift += 8;
                }

                if (shift < 0)
                    tmp >>= -shift;
                else
                    tmp <<= shift;
            }

            /* Write sample */
            result = FDKfwrite_EL2(&tmp, bps >> 3, 1, wav.buffer);
            if (result <= 0) {
                FDKprintfErr("WAV_OutputWrite(): error: unable to write to file\n");
                return -1;
            }
        }
    }
    else {
        for (i = 0; i < numberOfSamples; i++) {
            int result;
            double d;

            if (nSigBits == NBITS_FLOAT) {
                d = *fptr++;
            }
            else if (nSigBits == NBITS_DOUBLE) {
                d = *dptr++;
            }
            else {
                switch (nBufBits) {
                case 8:
                    tmp = *bptr++;
                    break;
                case 16:
                    tmp = *sptr++;
                    break;
                case 32:
                    tmp = *lptr++;
                    break;
                default:
                    return -1;
                }

                d = (double)tmp / (1U << (nSigBits - 1));
            }

            /* Write sample */
            if (bps == 64) {
                result = FDKfwrite_EL2(&d, 8, 1, wav.buffer);
            }
            else {
                float f = (float)d;
                result = FDKfwrite_EL2(&f, 4, 1, wav.buffer);
            }

            if (result <= 0) {
                FDKprintfErr("WAV_OutputWrite(): error: unable to write to file\n");
                return -1;
            }
        }
    }

    wav.header.ds64Chunk.dataSize64 += (numberOfSamples * (bps >> 3));
    return 0;
}

/**
 * WAV_OutputClose
 * \brief Close WAV Output handle
 * \param pWav pointer to WAV handle. *pWav is set to NULL.
 */

void WAV_OutputFlush2(WAV2& wav) {
    uint32_t size = 0;


    if (wav.header.ds64Chunk.dataSize64 + WAV_HEADER_SIZE >= (1LL << 32)) {
        /* File is >=4GB --> write RF64 Header */
        FDKstrncpy(wav.header.riffType, "RF64", 4);
        FDKstrncpy(wav.header.ds64Chunk.chunkID, "ds64", 4);
        wav.header.ds64Chunk.riffSize64 =
            LittleEndian64(wav.header.ds64Chunk.dataSize64 + WAV_HEADER_SIZE -
                (sizeof(wav.header.riffType) + sizeof(wav.header.riffSize)));
        wav.header.ds64Chunk.samplesPerCh64 =
            LittleEndian64(wav.header.ds64Chunk.dataSize64 /
                (wav.header.numChannels * (wav.header.bitsPerSample >> 3)));
        wav.header.ds64Chunk.dataSize64 = LittleEndian64(wav.header.ds64Chunk.dataSize64);
        wav.header.dataSize = LittleEndian32(0xFFFFFFFF);
        wav.header.riffSize = LittleEndian32(0xFFFFFFFF);
    }
    else {
        /* File is <4GB --> write RIFF Header */
        FDKstrncpy(wav.header.riffType, "RIFF", 4);
        FDKstrncpy(wav.header.ds64Chunk.chunkID, "JUNK", 4);
        wav.header.dataSize = (uint32_t)(wav.header.ds64Chunk.dataSize64 & 0x00000000FFFFFFFFLL);
        wav.header.riffSize =
            LittleEndian32(wav.header.dataSize + WAV_HEADER_SIZE -
                (sizeof(wav.header.riffType) + sizeof(wav.header.riffSize)));
        wav.header.dataSize = LittleEndian32(wav.header.dataSize);
        wav.header.ds64Chunk.chunkSize = LittleEndian32(28);
        wav.header.ds64Chunk.riffSize64 = LittleEndian64(0);
        wav.header.ds64Chunk.dataSize64 = LittleEndian64(0);
        wav.header.ds64Chunk.samplesPerCh64 = LittleEndian64(0);
        wav.header.ds64Chunk.tableLength = LittleEndian32(0);
    }

    size = WAV_WriteHeader(wav);
}
