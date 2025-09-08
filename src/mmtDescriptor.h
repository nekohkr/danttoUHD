#pragma once
#include <cstdint>
#include "stream.h"
#include <vector>
#include <optional>
#include <memory>

namespace atsc3 {

class MmtDescriptorBase {
public:
    virtual ~MmtDescriptorBase() = default;
    virtual uint8_t tag() const = 0;
    virtual bool unpack(Common::ReadStream& stream) = 0;
};

template<uint8_t Tag>
class MmtDescriptorTemplate : public MmtDescriptorBase {
public:
    static constexpr uint8_t kDescriptorTag = Tag;
    uint8_t tag() const override { return Tag; }
};

class MmtMpuTimestampDescriptor : public MmtDescriptorTemplate<0x01> {
public:
    bool unpack(Common::ReadStream& stream) override;

public:
    uint16_t descriptorLength;
    struct Entry {
        uint32_t mpuSequenceNumber;
        uint64_t mpuPresentationTime;

    };

    std::vector<Entry> entries;
};

class MmtDescriptors {
public:
    bool unpack(Common::ReadStream& stream);

public:
    std::vector<std::unique_ptr<MmtDescriptorBase>> descriptors;
};
}