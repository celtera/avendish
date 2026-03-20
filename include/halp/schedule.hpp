#pragma once

#include <halp/modules.hpp>

#include <cstdint>
#include <functional>

HALP_MODULE_EXPORT
namespace halp
{
template<typename... Args>
using scheduler_fun = std::function<void(int64_t, void(*)(Args...))>;
}
