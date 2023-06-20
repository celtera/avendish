#pragma once
#include <halp/inline.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>
#include <halp/value_types.hpp>

#include <string_view>
#include <type_traits>

namespace halp
{
template <typename T>
struct range_t
{
  constexpr range_t() = default;
  constexpr range_t(const range_t&) = default;
  constexpr range_t(range_t&&) = default;
  constexpr range_t& operator=(const range_t&) = default;
  constexpr range_t& operator=(range_t&&) = default;
  constexpr explicit range_t(auto&&...) { }

  static constexpr auto to_range() noexcept { return range_t{}; }
};
using range = range_t<long double>;
using irange = range_t<int>;
template <typename T>
inline constexpr auto default_range = range{};
template <>
inline constexpr auto default_range<int> = range{};
template <typename T>
inline constexpr auto default_irange = irange{};

template <typename T>
using init_range_t = range_t<T>;

/// Sliders ///
template <typename T, static_string lit, auto setup>
struct slider_t
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  T value{};

  operator T&() noexcept { return value; }
  operator const T&() const noexcept { return value; }
  auto& operator=(T t) noexcept
  {
    value = t;
    return *this;
  }
};

/// Time chooser ///
template <typename T, static_string lit, auto setup>
struct time_chooser_t : slider_t<T, lit, setup>
{
  enum widget
  {
    time_chooser
  };
};

/// Toggle ///
using toggle_setup = range_t<bool>;

inline constexpr auto default_toggle = toggle_setup{};

/// XY position ///
template <typename T, static_string lit, auto setup>
struct xy_pad_t
{
  using value_type = xy_type<T>;
  enum widget
  {
    xy
  };
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  value_type value = {};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

template <typename T, static_string lit, auto setup>
struct xy_spinboxes_t
{
  using value_type = xy_type<T>;
  enum widget
  {
    xy_spinbox,
    spinbox
  };
  static clang_buggy_consteval auto name() noexcept
  {
    return std::string_view{lit.value};
  }

  value_type value = {};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

template <typename T, static_string lit, auto setup>
struct xyz_spinboxes_t
{
  using value_type = xyz_type<T>;
  enum widget
  {
    xyz,
    spinbox
  };
  static clang_buggy_consteval auto name() noexcept
  {
    return std::string_view{lit.value};
  }

  value_type value = {};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

/// 1D range ///
template <typename T>
using range_slider_range = range_t<T>;

template <typename T, static_string lit, auto setup>
struct range_slider_t
{
  using value_type = range_slider_value<T>;
  enum widget
  {
    hrange_slider
  };
  static clang_buggy_consteval auto name() noexcept
  {
    return std::string_view{lit.value};
  }

  value_type value{};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};
template <typename T, static_string lit, auto setup>
struct range_spinbox_t  = range_slider_t<T, lit, setup>;

/// RGBA color ///
using color_init = init_range_t<color_type>;

template <static_string lit, auto setup>
struct color_chooser
{
  using value_type = color_type;
  enum widget
  {
    color
  };
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  value_type value{};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

}
