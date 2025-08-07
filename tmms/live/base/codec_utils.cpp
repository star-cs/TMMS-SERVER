#include "codec_utils.h"

namespace tmms::live
{
bool CodecUtils::IsCodecHeader(const PacketPtr& packet)
{
    // 第一个字节是FLV
    if (packet->PacketSize() > 1)
    {
        const char* b = packet->Data() + 1;
        if (*b == 0)
        {
            return true;
        }
    }

    return false;
}

} // namespace tmms::live