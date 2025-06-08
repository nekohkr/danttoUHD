#include "lls.h"
#include "stream.h"

namespace ATSC3 {

bool LLS::unpack(Common::ReadStream& stream)
{
    tableId = stream.get8U();
    groupId = stream.get8U();
    groupCount = stream.get8U();
    tableVersion = stream.get8U();

    return true;
}

}