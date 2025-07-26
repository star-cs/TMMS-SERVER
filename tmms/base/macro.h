/*
 * @Author: star-cs
 * @Date: 2025-07-21 20:10:13
 * @LastEditTime: 2025-07-26 12:12:47
 * @FilePath: /TMMS-SERVER/tmms/base/macro.h
 * @Description:
 */
#pragma once
#include "base/log/log.h"
#include "base/utils/utils.h"
#include <assert.h>

#if defined __GNUC__ || defined __llvm__
    /// CORE_LIKELY 宏的封装, 告诉编译器优化,条件大概率成立
    #define CORE_LIKELY(x) __builtin_expect(!!(x), 1)
    /// CORE_UNLIKELY 宏的封装, 告诉编译器优化,条件大概率不成立
    #define CORE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define CORE_LIKELY(x)   (x)
    #define CORE_UNLIKELY(x) (x)
#endif

#define CORE_ASSERT(x)                                                                                                 \
    if (!(x))                                                                                                          \
    {                                                                                                                  \
        CORE_ERROR("ASSERTION : {} \nbacktrace:\n {} ", #x, tmms::base::BacktraceToString(100, 2, "         "));       \
        assert(x);                                                                                                     \
    }

#define CORE_ASSERT2(x, w)                                                                                             \
    if (!(x))                                                                                                          \
    {                                                                                                                  \
        CORE_ERROR("ASSERTION : {}\n{}\nbacktrace:\n{}", #x, w, tmms::base::BacktraceToString(100, 2, "         "));   \
        assert(x);                                                                                                     \
    }
