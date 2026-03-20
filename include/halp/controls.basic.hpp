#pragma once
#include <halp/inline.hpp>
#include <halp/modules.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <string_view>
#include <type_traits>
HALP_MODULE_EXPORT
namespace halp
{
/// Basic value port
template <static_string lit, typename T>
struct val_port
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

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

template <static_string lit, typename T>
  requires std::is_trivial_v<T>
struct val_port<lit, T>
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  operator T&() noexcept { return value; }
  operator const T&() const noexcept { return value; }
  auto& operator=(T t) noexcept
  {
    value = t;
    return *this;
  }

  // Running value (last value before the tick started)
  T value{};
};
}
