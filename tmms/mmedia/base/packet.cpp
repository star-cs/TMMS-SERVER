#include "packet.h"
#include <cstring>

namespace tmms::mm
{
Packet::ptr Packet::NewPacket(int32_t size)
{
    auto    block_size = size + sizeof(Packet);
    Packet* packet     = (Packet*)new char[block_size];
    memset((void*)packet, 0x00, block_size);
    packet->index_    = -1;
    packet->type_     = kPacketTypeUnKnowed;
    packet->capacity_ = size;
    packet->ext_.reset();

    return Packet::ptr(packet, [](Packet* p) { delete[] (char*)p; });
}
} // namespace tmms::mm