#pragma once


#include <vector>

#define SPEAKER_FRONT_LEFT 0x1
#define SPEAKER_FRONT_RIGHT 0x2
#define SPEAKER_FRONT_CENTER 0x4
#define SPEAKER_LOW_FREQUENCY 0x8
#define SPEAKER_BACK_LEFT 0x10
#define SPEAKER_BACK_RIGHT 0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER 0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER 0x80
#define SPEAKER_BACK_CENTER 0x100
#define SPEAKER_SIDE_LEFT 0x200
#define SPEAKER_SIDE_RIGHT 0x400
#define SPEAKER_TOP_CENTER 0x800
#define SPEAKER_TOP_FRONT_LEFT 0x1000
#define SPEAKER_TOP_FRONT_CENTER 0x2000
#define SPEAKER_TOP_FRONT_RIGHT 0x4000
#define SPEAKER_TOP_BACK_LEFT 0x8000
#define SPEAKER_TOP_BACK_CENTER 0x10000
#define SPEAKER_TOP_BACK_RIGHT 0x20000
#define SPEAKER_RESERVED 0x80000000

#define NBITS_FLOAT (-32)
#define NBITS_DOUBLE (-64)

/*!
    * RF64 WAVE file struct, for being able to read WAV files >4GB.
    */
typedef struct Ds64Header {
    char chunkID[4];
    uint32_t chunkSize;
    uint64_t riffSize64;
    uint64_t dataSize64;
    uint64_t samplesPerCh64;
    uint32_t tableLength;
} Ds64Header;

#define WAV_HEADER_SIZE 80 /* including DS64Header */

/*!
    * RIFF WAVE file struct.
    * For details see WAVE file format documentation (for example at http://www.wotsit.org).
    */
typedef struct WAV_HEADER {
    char riffType[4];
    uint32_t riffSize;
    char waveType[4];
    char formatType[4];
    uint32_t formatSize;
    uint16_t compressionCode;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t bytesPerSecond;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char dataType[4];
    uint32_t dataSize;
    Ds64Header ds64Chunk;
} WAV_HEADER;

struct WAV2 {
    WAV_HEADER header;
    std::vector<uint8_t> buffer; // with header
    std::vector<uint8_t> data; // without header
    uint32_t channelMask;
    uint32_t headerSize;
};


/**
    * \brief  Open WAV output/writer handle.
    *
    * \param pWav            Pointer to WAV handle to be returned.
    * \param outputFilename  File name of the file to be written to.
    * \param sampleRate      Desired samplerate of the resulting WAV file.
    * \param numChannels     Desired number of audio channels of the resulting WAV file.
    * \param bitsPerSample   Desired number of bits per audio sample of the resulting WAV file.
    *
    * \return  0: ok; -1: error
    */
int WAV_OutputOpen2(WAV2& pWav, const char* outputFilename, int sampleRate, int numChannels,
    int bitsPerSample);

/**
    * \brief  Write data to WAV file asociated to WAV handle.
    *
    * \param wav              Handle of WAV file
    * \param sampleBuffer     Pointer to audio samples, right justified integer values.
    * \param numberOfSamples  The number of individual audio sample valuesto be written.
    * \param nBufBits         Size in bits of each audio sample in sampleBuffer.
    * \param nSigBits         Amount of significant bits of each nBufBits in sampleBuffer.
    *
    * \return 0: ok; -1: error
    */
int WAV_OutputWrite2(WAV2& wav, void* sampleBuffer, uint32_t numberOfSamples, int nBufBits,
    int nSigBits);

/**
    * \brief       Close WAV output handle.
    * \param pWav  Pointer to WAV handle. *pWav is set to NULL.
    */
void WAV_OutputClose2(WAV2& wav);
void WAV_OutputFlush2(WAV2& pWav);
