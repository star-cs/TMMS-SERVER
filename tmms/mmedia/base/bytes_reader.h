/*
 * @Author: star-cs
 * @Date: 2025-07-28 15:16:11
 * @LastEditTime: 2025-07-28 15:16:23
 * @FilePath: /TMMS-SERVER/tmms/mmedia/base/byte_reader.h
 * @Description:
 */
#pragma once
#include <stdint.h>

namespace tmms::mm
{

class BytesReader
{
public:
    BytesReader()  = default;
    ~BytesReader() = default;
    static uint64_t ReadUint64T(const char* data);
    static uint32_t ReadUint32T(const char* data);
    static uint32_t ReadUint24T(const char* data);
    static uint16_t ReadUint16T(const char* data);
    static uint8_t  ReadUint8T(const char* data);
};
} // namespace tmms::mm