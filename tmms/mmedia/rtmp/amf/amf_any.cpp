#include "amf_any.h"
#include "base/log/log.h"
#include "mmedia/base/bytes_reader.h"
#include "mmedia/base/bytes_writer.h"
#include <cstring>
#include <netinet/in.h>
#include <string>
namespace
{
static std::string empty_string;
}

namespace tmms::mm
{
AMFAny::AMFAny(const std::string& name) : name_(name)
{
}
AMFAny::AMFAny()
{
}

AMFAny::~AMFAny()
{
}

const std::string& AMFAny::String()
{
    if (this->IsString())
    {
        return this->String();
    }
    RTMP_ERROR("AMF is not a string");
    return empty_string;
}

bool AMFAny::Boolean()
{
    if (this->IsBoolean())
    {
        return this->Boolean();
    }
    RTMP_ERROR("AMF is not a boolean");
    return false;
}

double AMFAny::Number()
{
    if (this->IsNumber())
    {
        return this->Number();
    }
    RTMP_ERROR("AMF is not a number");
    return 0.0;
}

double AMFAny::Date()
{
    if (this->IsDate())
    {
        return this->Date();
    }
    RTMP_ERROR("AMF is not a date");
    return 0.0;
}

AMFObjectPtr AMFAny::Object()
{
    if (this->IsObject())
    {
        return this->Object();
    }
    RTMP_ERROR("AMF is not a object");
    return AMFObjectPtr();
}

/// @brief 解析string类型的数据
/// @param data
/// @return string数据
std::string AMFAny::DecodeString(const char* data)
{
    // string 类型 = 0x02 + UTF-8编码的字符串
    // UTF-8编码的字符串由16位的长度+字符串内容
    auto len = BytesReader::ReadUint16T(data);
    if (len > 0)
    {
        std::string str(data + 2, len);
        return str;
    }
    return std::string();
}

int32_t AMFAny::EncodeName(char* output, const std::string& name)
{
    char* old = output;
    auto  len = name.size();

    unsigned short length = htons(len);
    memcpy(output, &length, 2);
    output += 2;

    memcpy(output, name.c_str(), len);
    output += len;
    return len + 2;
}

int32_t AMFAny::EncodeNumber(char* output, double dVal)
{
    char* p = output;
    *p++    = kAMFNumber;      // 一个字节的类型
    p += WriteNumber(p, dVal); // 8个字节数据
    return p - output;
}

int32_t AMFAny::EncodeString(char* output, const std::string& str)
{
    char* p   = output;
    auto  len = str.size();
    *p++      = kAMFString;                 // 一字节类型
    p += BytesWriter::WriteUint16T(p, len); // 2字节数据长度
    memcpy(p, str.c_str(), len);
    p += len;
    return p - output;
}

int32_t AMFAny::EncodeBoolean(char* output, bool b)
{
    char* p = output;
    *p++    = kAMFBoolean;
    *p++    = b ? 0x01 : 0x00;
    return p - output;
}

int32_t AMFAny::EncodeNamedBoolean(char* output, const std::string& name, bool bVal)
{
    char* old = output;
    output += EncodeName(output, name);
    output += EncodeBoolean(output, bVal);
    return output - old;
}

int32_t AMFAny::EncodeNamedNumber(char* output, const std::string& name, double dVal)
{
    char* old = output;

    output += EncodeName(output, name); // 先写name
    output += EncodeNumber(output, dVal);
    return output - old;
}

int32_t AMFAny::EncodeNamedString(char* output, const std::string& name, const std::string& str)
{
    char* old = output;

    output += EncodeName(output, name); // 先写name
    output += EncodeString(output, str);
    return output - old;
}

/// @brief  用于将一个双精度浮点数 value 写入到字符数组 buf 中，用于发送，要转换成网络大端
/// @param buf
/// @param value
/// @return 写入的字节数
int AMFAny::WriteNumber(char* buf, double value)
{
    uint64_t res;
    uint64_t in;
    memcpy(&in, &value, sizeof(double));
    // __bswap_64 是一个系统级的函数或宏（通常是宏），用于将 64 位整数 in
    // 的字节序从主机字节序转换为网络字节序（大端字节序）。
    res = __bswap_64(in);
    memcpy(buf, &res, 8);
    return 8;
}


} // namespace tmms::mm