#include "lgContainer.h"
#include "stream.h"

void LgContainerUnpacker::addBuffer(const std::vector<uint8_t>& data) {
    buffer.insert(buffer.end(), data.begin(), data.end());
}

void LgContainerUnpacker::unpack(const UnpackCallback& callback) {
    constexpr uint32_t kLgContainerSyncWord = 0x5A5A5A5A;
    size_t pos = 0;

    Common::ReadStream stream(buffer);
    while (stream.leftBytes() > 17) {
        uint32_t syncWord = stream.peekBe32U();
        if (syncWord != kLgContainerSyncWord) {
            stream.skip(1);
            continue;
        }

        pos = stream.getPos();

        stream.skip(4); // skip sync word

        uint8_t uint8 = stream.get8U();

        LgContainer container;
        container.syncWord = syncWord;
        container.errorMode = (uint8 & 0b10000000) >> 7;
        container.error = (uint8 & 0b01000000) >> 6;
        container.plpId = uint8 & 0b00111111;

        container.size = stream.getBe16U();
        container.cc = stream.get8U();
        container.t_mode = stream.get8U();
        container.time_value = stream.getBe64U();

        if (container.size + 0x23 > stream.leftBytes()) {
            break;
        }

        container.payload.resize(container.size);
        stream.read(container.payload.data(), container.size);
        
        callback(container);

        pos = stream.getPos();
    }

    if (pos != 0) {
        buffer.erase(buffer.begin(), buffer.begin() + pos);
    }
    return;
}

void LgContainerUnpacker::clear()
{
    buffer.clear();
}
