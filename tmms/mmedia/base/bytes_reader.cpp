/*
 * @Author: star-cs
 * @Date: 2025-07-28 15:16:35
 * @LastEditTime: 2025-07-28 15:18:41
 * @FilePath: /TMMS-SERVER/tmms/mmedia/base/bytes_reader.cpp
 * @Description:
 */

#include "bytes_reader.h"
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
using namespace tmms::mm;

uint64_t BytesReader::ReadUint64T(const char* data)
{
    uint64_t in = *((uint64_t*)data);
    // return ntohl(*c); bug:ntol是转为32位的int，丢失了
    uint64_t res = __bswap_64(in); // 大端转为小端
    double   value;
    memcpy(&value, &res, sizeof(double));
    return value;
}

uint32_t BytesReader::ReadUint32T(const char* data)
{
    uint32_t* c = (uint32_t*)data;
    return ntohl(*c);
}

/// @brief
/// @param data
/// @return
uint32_t BytesReader::ReadUint24T(const char* data)
{
    unsigned char* c = (unsigned char*)data;
    uint32_t       val;
    val = (c[0] << 16) | (c[1] << 8) | c[2]; // 组装24位3字节
    return val;                              // 已经是小端了
}

/// @brief 读取缓存头部的两个字节值
/// @param data
/// @return 两个字节的short int值
uint16_t BytesReader::ReadUint16T(const char* data)
{
    uint16_t* c = (uint16_t*)data;
    return ntohs(*c);
}

/// @brief 读取缓存头部一个字节的值
/// @param data 网络数据缓存
/// @return 一个字节的值
uint8_t BytesReader::ReadUint8T(const char* data)
{
    return data[0];
}