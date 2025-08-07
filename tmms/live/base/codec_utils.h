#pragma once
#include "mmedia/base/packet.h"

namespace tmms::live
{
using namespace tmms::mm;

class CodecUtils
{
public:
    static bool IsCodecHeader(const PacketPtr& packet);
};

} // namespace tmms::live