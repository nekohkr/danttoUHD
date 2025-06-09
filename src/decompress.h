#pragma once
#include <vector>
#include <cstdint>
#include <optional>
#include <string>

std::optional<std::string> gzipInflate(const std::vector<uint8_t>& input);