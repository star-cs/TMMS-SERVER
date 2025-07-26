/*
 * @Author: star-cs
 * @Date: 2025-07-26 14:07:56
 * @LastEditTime: 2025-07-26 15:05:07
 * @FilePath: /TMMS-SERVER/tmms/base/atomic_queue.hpp
 * @Description:
 */
#pragma once

#include "atomic_queue/atomic_queue.h"
#include "base/config/config.h"
#include <cstddef>

namespace tmms::base
{
// lib AtomicQueue
template<class Queue, size_t Capacity>
struct CapacityToConstructor : Queue
{
    CapacityToConstructor() : Queue(Capacity) {}
};

/**
 * @brief mpmc atomic_queue
 *
 * @tparam T storage_type
 *
 * @note true, false, false: MAXIMIZE_THROUGHPUT = true, TOTAL_ORDER = false, bool SPSC = false
 */
template<typename T>
using AtomicQueue =
    CapacityToConstructor<atomic_queue::AtomicQueueB2<T, std::allocator<T>, true, false, false>, config::kQueCap>;

} // namespace tmms::base