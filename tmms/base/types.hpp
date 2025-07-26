/*
 * @Author: star-cs
 * @Date: 2025-07-26 19:56:45
 * @LastEditTime: 2025-07-26 19:57:36
 * @FilePath: /TMMS-SERVER/tmms/base/types.hpp
 * @Description:
 */
#pragma once

#include <cstdint>
namespace tmms::base
{

enum class schedule_strategy : uint8_t
{
    fifo, // default
    lifo,
    none
};

enum class dispatch_strategy : uint8_t
{
    round_robin,
    none
};

enum class memory_allocator_type : uint8_t
{
    std_allocator,
    none
};

}; // namespace tmms::base