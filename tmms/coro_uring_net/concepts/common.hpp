#pragma once

#include <type_traits>

namespace tmms::concepts
{
template<typename T>
concept pod_type = std::is_standard_layout_v<T> && std::is_trivial_v<T>;

}