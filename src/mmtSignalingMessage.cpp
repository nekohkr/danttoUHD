#include "mmtSignalingMessage.h"

namespace atsc3 {

bool MmtSignalingMessage::unpack(Common::ReadStream& stream) {
    messageId = stream.getBe16U();
    version = stream.get8U();

    return true;
}

bool MmtPaMessage::unpack(Common::ReadStream& stream) {
    if (!MmtSignalingMessage::unpack(stream)) {
        return false;
    }

    length = stream.getBe32U();
    if (stream.leftBytes() < length) {
        return false;
    }

    tableCount = stream.get8U();
    for (int i = 0; i < tableCount; i++) {
        struct TableInfo tableInfo;
        tableInfo.id = stream.get8U();
        tableInfo.version = stream.get8U();
        tableInfo.length = stream.getBe16U();

        tableInfos.push_back(tableInfo);
    }

    return true;
}

bool MmtMpiMessage::unpack(Common::ReadStream& stream) {
    if (!MmtSignalingMessage::unpack(stream)) {
        return false;
    }
    length = stream.getBe32U();
    return true;
}

bool MmtMptMessage::unpack(Common::ReadStream& stream) {
    if (!MmtSignalingMessage::unpack(stream)) {
        return false;
    }
    length = stream.getBe16U();
    return true;
}

}