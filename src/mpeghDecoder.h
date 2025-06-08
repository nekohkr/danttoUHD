#pragma once
#include <vector>
#include <string>

std::vector<uint8_t> encodeAAC(std::vector<uint8_t> wav, int32_t channels, uint32_t sample_rate);
int mpeghDecode(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);