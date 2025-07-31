#include "amf_base.h"
#include "base/log/log.h"

namespace tmms::mm
{
int AMFBoolean::Decode(const char* data, int size, bool has)
{
    if (size >= 1)
    {
        b_ = *data != 0 ? true : false;
        return 1;
    }
    return -1;
}

void AMFBoolean::Dump() const
{
    RTMP_TRACE("Boolean:{}", b_);
}

int AMFDate::Decode(const char* data, int size, bool has)
{
    // Date类型 = 0x0b + DOUBLE(8bytes) + time-zene(2bytes)
    if (size < 10)
    {
        return -1;
    }

    utc_ = BytesReader::ReadUint64T(data);
    data += 8;
    utc_offset_ = BytesReader::ReadUint16T(data);
    return 10;
}

void AMFDate::Dump() const
{
    RTMP_TRACE("Date:{}", utc_);
}

// @brief longstring类型由len + data. len4 字节
/// @param data
/// @param size
/// @param has
/// @return
int AMFLongString::Decode(const char* data, int size, bool has)
{
    // 够不够解析长度
    if (size < 4)
    {
        return -1;
    }
    auto len = BytesReader::ReadUint32T(data);
    if (len < 0 || size < len + 4)
    {
        return -1;
    }
    longString_.assign(data + 4, len);
    return 4 + len;
}

void AMFLongString::Dump() const
{
    RTMP_TRACE("longstring:{}", longString_);
}

// @brief 解析NUmber类型的数据
/// @param data
/// @param size
/// @param has
/// @return 返回读取成功的字节数，失败返回-1
int AMFNumber::Decode(const char* data, int size, bool has)
{
    if (size >= 8)
    {
        number_ = BytesReader::ReadUint64T(data);
        return 8;
    }
    return -1;
}

void AMFNumber::Dump() const
{
    RTMP_TRACE("number:{}", number_);
}

/// @brief string类型由len + data. len2字节
/// @param data
/// @param size
/// @param has  // 有没有属性名
/// @return
int AMFString::Decode(const char* data, int size, bool has)
{
    // 够不够解析长度
    if (size < 2)
    {
        return -1;
    }
    auto len = BytesReader::ReadUint16T(data);

    if (len < 0 || size < len + 2)
    {
        return -1;
    }
    string_ = DecodeString(data);
    return 2 + string_.size();
}

void AMFString::Dump() const
{
    RTMP_TRACE("string:{}", string_);
}

} // namespace tmms::mm