#pragma once
#include <cstdint>
#include <vector>
#include <optional>
#include <unordered_map>
#include "mmt.h"

namespace atsc3 {

class MmtAssembler {
public:
    std::optional<std::vector<uint8_t>> addFragment(uint32_t packetId, uint32_t mpuSequenceNumber,
        MmtFragmentationIndicator fragmentationIndicator, const std::vector<uint8_t>& payload) {
        auto& entry = entities[packetId];

        if (entry.buffer.size() == 0) {
            if (fragmentationIndicator == MmtFragmentationIndicator::MiddleFragment) {
                entry.buffer.clear();
                return std::nullopt;
            }

            entry.currentMpuSequenceNumber = mpuSequenceNumber;
        }
        else {
            if (fragmentationIndicator == MmtFragmentationIndicator::FirstFragment) {
                entry.buffer.clear();
                return std::nullopt;
            }

            if (entry.currentMpuSequenceNumber != mpuSequenceNumber) {
                entry.buffer.clear();
                return std::nullopt;
            }
        }

        entry.buffer.insert(entry.buffer.end(), payload.begin(), payload.end());

        if (fragmentationIndicator == MmtFragmentationIndicator::LastFragment) {
            return std::move(entry.buffer);
        }

        return std::nullopt;
    }

private:
    struct Entry {
        uint32_t currentMpuSequenceNumber;
        std::vector<uint8_t> buffer;
    };
    std::unordered_map<uint32_t, Entry> entities;

};

}