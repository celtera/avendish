#pragma once

#include <functional>
#include <cstdint>

namespace halp
{
template<typename... Args>
using scheduler_fun = std::function<void(int64_t, void(*)(Args...))>;
}
