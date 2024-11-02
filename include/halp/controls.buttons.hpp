#pragma once
#include <halp/inline.hpp>
#include <halp/modules.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>
#include <halp/value_types.hpp>

#include <string_view>
#include <type_traits>
HALP_MODULE_EXPORT
namespace halp
{

template <static_string lit>
struct maintained_button_t
{
  enum widget
  {
    button,
    pushbutton
  };
  using range = dummy_range;

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  bool value = false;
  operator bool&() noexcept { return value; }
  operator const bool&() const noexcept { return value; }
  auto& operator=(bool t) noexcept
  {
    value = t;
    return *this;
  }
};

template <static_string lit>
struct impulse_button_t
{
  enum widget
  {
    bang,
    impulse
  };
  using range = impulse_type;
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  std::optional<impulse_type> value;
  operator bool() const noexcept { return bool(value); }
};
}
