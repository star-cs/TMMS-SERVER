#pragma once
#include "amf_any.h"
#include "base/log/log.h"
#include <memory>
#include <vector>

namespace tmms::mm
{

/// @brief 继承AMFAny，实现Object数据类型的解析
class AMFObject : public AMFAny
{
public:
    AMFObject(const std::string& name) : AMFAny(name) {}
    AMFObject() {}
    ~AMFObject() {}

public:
    /// @brief 一个object是键值对，可以解析多个属性
    /// @param data
    /// @param size
    /// @param has  有没有属性名，就是是不是object对象
    /// @return
    int Decode(const char* data, int size, bool has = false) override;

    bool         IsObject() override { return true; }
    AMFObjectPtr Object() override { return std::dynamic_pointer_cast<AMFObject>(shared_from_this()); }

    void Dump() const override;

    int              DecodeOnce(const char* data, int size, bool has = false);
    const AMFAnyPtr& Property(const std::string& name) const;
    const AMFAnyPtr& Property(int index) const;

private:
    std::vector<AMFAnyPtr> properties_;
};

} // namespace tmms::mm