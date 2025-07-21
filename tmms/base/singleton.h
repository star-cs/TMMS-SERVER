/*
 * @Author: star-cs
 * @Date: 2025-07-21 13:39:45
 * @LastEditTime: 2025-07-21 14:02:10
 * @FilePath: /TMMS-SERVER/mms/base/singleton.h
 * @Description:
 */
#pragma once

#include <memory>
namespace tmms::base
{
template<class T, class X = void, int N = 0>
class Singleton
{
public:
    static T* GetInstance()
    {
        static T v;
        return &v;
    }
};

template<class T, class X = void, int N = 0>
class SingletonPtr
{
public:
    static std::shared_ptr<T> GetInstance()
    {
        static std::shared_ptr<T> v(new T);
        return v;
    }
};
} // namespace tmms::base