#pragma once
#include <halp/modules.hpp>
#include <halp/static_string.hpp>

#include <string_view>
HALP_MODULE_EXPORT
namespace halp
{

template <halp::static_string lit, typename T>
struct addr_port
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  static clang_buggy_consteval auto path() { return std::string_view{lit.value}; }
  operator T&() noexcept { return value; }
  operator const T&() const noexcept { return value; }
  auto& operator=(const T& t) noexcept
  {
    value = t;
    return *this;
  }
  auto& operator=(T&& t) noexcept
  {
    value = std::move(t);
    return *this;
  }

  // Running value (last value before the tick started)
  T value{};
};

}
