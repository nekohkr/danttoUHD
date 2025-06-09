#pragma once
#include <vector>
#include <cstdint>

void* wav_read_open(const std::vector<uint8_t>& input);
void wav_read_close(void* obj);

int wav_get_header(void* obj, int* format, int* channels, int* sample_rate, int* bits_per_sample, unsigned int* data_length);
int wav_read_data(void* obj, unsigned char* data, unsigned int length);
