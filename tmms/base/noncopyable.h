/*
 * @Author: star-cs
 * @Date: 2025-07-19 23:09:58
 * @LastEditTime: 2025-07-21 14:02:17
 * @FilePath: /TMMS-SERVER/mms/base/noncopyable.h
 * @Description:
 */
#pragma once

namespace tmms::base
{
class NonCopyable
{
protected:
    NonCopyable() {}
    ~NonCopyable() {}

    // c++  3/5法则，有默认的拷贝构造，拷贝赋值运算符，析构，移动构造，移动赋值
    // 定义了其中一个，默认生成其他四个。删除其中一个，默认不会生成其他四个
    NonCopyable(const NonCopyable&)            = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};
} // namespace mms::base