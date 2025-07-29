/*
 * @Author: star-cs
 * @Date: 2025-07-28 15:17:37
 * @LastEditTime: 2025-07-28 15:17:49
 * @FilePath: /TMMS-SERVER/tmms/mmedia/base/byte_writer.cpp
 * @Description:
 */
#include "bytes_writer.h"

#include <cstring>
#include <netinet/in.h>

using namespace tmms::mm;

int BytesWriter::WriteUint32T(char* buf, uint32_t val)
{
    val = htonl(val);
    memcpy(buf, &val, sizeof(int32_t));
    return sizeof(int32_t);
}

int BytesWriter::WriteUint24T(char* buf, uint32_t val)
{
    val       = htonl(val);
    char* ptr = (char*)&val;
    ptr++; // 主机小端，网络大端
    memcpy(buf, ptr, 3);
    return 3;
}

int BytesWriter::WriteUint16T(char* buf, uint16_t val)
{
    val = htonl(val);
    memcpy(buf, &val, sizeof(int16_t));
    return sizeof(int16_t);
}

/// @brief 转换成网络字节序（大端）
/// @param buf
/// @param val
/// @return 转换的字节
int BytesWriter::WriteUint8T(char* buf, uint8_t val)
{
    char* data = (char*)&val;
    buf[0]     = data[0];
    return 1;
}