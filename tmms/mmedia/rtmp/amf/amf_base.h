#pragma once
#include "mmedia/base/bytes_reader.h"
#include "mmedia/rtmp/amf/amf_any.h"

namespace tmms::mm
{

class AMFBoolean : public AMFAny
{
public:
    AMFBoolean(const std::string& name) : AMFAny(name) {}
    AMFBoolean() {}
    ~AMFBoolean() {}

    int Decode(const char* data, int size, bool has = false) override;

    bool IsBoolean() override { return true; }
    bool Boolean() override { return b_; }
    void Dump() const override;

private:
    bool b_{false};
};

/// @brief 继承AMFAny，实现Date数据类型的解析
class AMFDate : public AMFAny
{
public:
    AMFDate(const std::string& name) : AMFAny(name) {}
    AMFDate() {}
    ~AMFDate() {}

    int Decode(const char* data, int size, bool has = false) override;

    bool   IsDate() override { return true; }
    double Date() override { return utc_; }

    void Dump() const override;

    int16_t UtcOffset() const { return utc_offset_; }

private:
    double  utc_{0.0f};
    int16_t utc_offset_{0}; // 时区 偏移
};

class AMFLongString : public AMFAny
{
public:
    AMFLongString(const std::string& name) : AMFAny(name) {}
    AMFLongString() {}
    ~AMFLongString() {}

    int Decode(const char* data, int size, bool has = false) override;

    bool               IsString() override { return false; }
    const std::string& String() override { return longString_; }
    void               Dump() const override;

private:
    std::string longString_;
};

class AMFNumber : public AMFAny
{
public:
    AMFNumber(const std::string& name) : AMFAny(name) {}
    AMFNumber() {}
    ~AMFNumber() {}

public:
    int Decode(const char* data, int size, bool has = false) override;

    bool   IsNumber() override { return true; }
    double Number() override { return number_; }

    void Dump() const override;

private:
    double number_{0};
};

class AMFString : public AMFAny
{
public:
    AMFString(const std::string& name) : AMFAny(name) {}
    AMFString() {}
    ~AMFString() {}

public:
    int                Decode(const char* data, int size, bool has = false) override;
    
    bool               IsString() override { return true; }
    const std::string& String() override { return string_; }
    void               Dump() const override;

private:
    std::string string_;
};

} // namespace tmms::mm