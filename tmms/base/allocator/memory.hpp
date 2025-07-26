/*
 * @Author: star-cs
 * @Date: 2025-07-26 10:36:50
 * @LastEditTime: 2025-07-26 19:41:37
 * @FilePath: /TMMS-SERVER/tmms/base/allocator/memory.hpp
 * @Description:
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include "base/types.hpp"

namespace tmms::base
{


template<memory_allocator_type alloc_strategy>
class memory_allocator
{
public:
    struct config
    {
    };

public:
    ~memory_allocator() = default;

    auto init(config config) -> void {}

    auto allocate(size_t size) -> void* { return nullptr; }

    auto release(void* ptr) -> void {}
};

/**
 * @brief std memory allocator
 *
 * @tparam
 */
template<>
class memory_allocator<memory_allocator_type::std_allocator>
{
public:
    struct config
    {
    };

public:
    ~memory_allocator() = default;

    auto init(config config) -> void {}

    auto allocate(size_t size) -> void* { return malloc(size); }

    auto release(void* ptr) -> void { free(ptr); }
};
} // namespace tmms::base
