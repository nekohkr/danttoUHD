#pragma once
#include <cstdio>
#include <cstdint>
#include <vector>
extern "C" {
#include <aacenc_lib.h>
}
#include "wavreader.h"

class AacEncoder {
public:
	int encode(const std::vector<uint8_t>& input, std::vector<std::vector<uint8_t>>& output);
	void close();

public:
	bool inited = false;
	int bitrate = 64000;
	int ch;
	const char* infile, * outfile;
	FILE* out;
	void* wav;
	int format, sample_rate, channels, bits_per_sample;
	int input_size;
	uint8_t* input_buf;
	int16_t* convert_buf;
	int aot = 2;
	int afterburner = 1;
	int eld_sbr = 0;
	int vbr = 0;
	HANDLE_AACENCODER handle;
	AACENC_InfoStruct info = { 0 };

};