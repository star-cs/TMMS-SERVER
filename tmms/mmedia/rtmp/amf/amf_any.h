#pragma once

#include <memory>
namespace tmms::mm
{

enum AMFDataType // 不会全部用到AMF携带的数据类型定义
{
    kAMFNumber = 0,
    kAMFBoolean,
    kAMFString,
    kAMFObject,
    kAMFMovieClip, /* reserved, not used */
    kAMFNull,
    kAMFUndefined,
    kAMFReference,
    kAMFEcmaArray,
    kAMFObjectEnd,
    kAMStrictArray,
    kAMFDate,
    kAMFLongString,
    kAMFUnsupported,
    kAMFRecordset, /* reserved, not used */
    kAMFXMLDoc,
    kAMFTypedObject,
    kAMFAvmplus, /* switch to AMF3 */
    kAMFInvalid = 0xff,
};

class AMFObject;
class AMFAny;
using AMFObjectPtr = std::shared_ptr<AMFObject>;
using AMFAnyPtr    = std::shared_ptr<AMFAny>;

class AMFAny : public std::enable_shared_from_this<AMFAny>
{
public:
    AMFAny(const std::string& name);
    AMFAny();
    virtual ~AMFAny();

    /// @brief 解码
    /// @param data  需要解析的数据缓存地址
    /// @param size  需要解析的数据缓存大小
    /// @param has   是否带名字
    /// @return >=0, 解析成功的字节数；-1，解析出错
    virtual int Decode(const char* data, int size, bool has = false) = 0;

    // 由子类具体实现，返回对应类型的数据
    virtual const std::string& String();
    virtual bool               Boolean();
    virtual double             Number();
    virtual double             Date();
    virtual AMFObjectPtr       Object();

    // 不是每个类都要重新实现的这个函数的，作为一个通用函数
    virtual bool IsString() { return false; }
    virtual bool IsNumber() { return false; }
    virtual bool IsBoolean() { return false; }
    virtual bool IsDate() { return false; }
    virtual bool IsObject() { return false; }

    virtual void       Dump() const = 0;
    const std::string& Name() const { return name_; }

    // @brief 获取类型的属性个数
    /// @return 基本类型都是1个属性，object有多个
    virtual int32_t Count() const { return 1; }

    // 数据封装，都是根据协议写入的
    static int32_t EncodeName(char* output, const std::string& name);
    static int32_t EncodeNumber(char* output, double dVal);
    static int32_t EncodeString(char* output, const std::string& str);
    static int32_t EncodeBoolean(char* output, bool b);
    static int32_t EncodeNamedBoolean(char* output, const std::string& name, bool bVal);
    static int32_t EncodeNamedNumber(char* output, const std::string& name, double dVal);
    static int32_t EncodeNamedString(char* output, const std::string& name, const std::string& str);

protected:
    static int         WriteNumber(char* buf, double value);
    static std::string DecodeString(const char* data);

private:
    std::string name_;
};
} // namespace tmms::mm