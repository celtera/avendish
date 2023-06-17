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
  T min, max, init;
};
using range = range_t<long double>;
using irange = range_t<int>;
template <typename T>
inline constexpr auto default_range = range{0., 1., 0.5};
template <>
inline constexpr auto default_range<int> = range{0., 127., 64.};
template <typename T>
inline constexpr auto default_irange = irange{0, 127, 64};

template <typename T>
struct init_range_t
{
  T init;
};
using init_range = init_range_t<long double>;

/// Sliders ///
template <typename T, static_string lit, auto setup>
struct slider_t
{
  struct range
  {
    const T min = T(setup.min);
    const T max = T(setup.max);
    const T init = T(setup.init);
  };

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  T value = setup.init;

  operator T&() noexcept { return value; }
  operator const T&() const noexcept { return value; }
  auto& operator=(T t) noexcept
  {
    value = t;
    return *this;
  }
};

/// Time chooser ///
template <typename T, static_string lit, range setup>
struct time_chooser_t
{
  enum widget
  {
    time_chooser
  };

  struct range
  {
    const T min = T(setup.min);
    const T max = T(setup.max);
    const T init = T(setup.init);
  };

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  T value = setup.init;

  operator T&() noexcept { return value; }
  operator const T&() const noexcept { return value; }
  auto& operator=(T t) noexcept
  {
    value = t;
    return *this;
  }
};

/// Toggle ///
struct toggle_setup
{
  const bool min = false;
  const bool max = true;
  bool init = false;
  clang_buggy_consteval range to_range() const noexcept
  {
    return range{0., 1., init ? 1. : 0.};
  }
};

inline constexpr auto default_toggle = toggle_setup{.init = false};

/// XY position ///
template <typename T, static_string lit, range setup>
struct xy_pad_t
{
  using value_type = xy_type<T>;
  enum widget
  {
    xy
  };
  static clang_buggy_consteval auto range()
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  value_type value = {setup.init, setup.init};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

template <typename T, static_string lit, range setup>
struct xy_spinboxes_t
{
  using value_type = xy_type<T>;
  enum widget
  {
    xy_spinbox,
    spinbox
  };
  static clang_buggy_consteval auto range() noexcept
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
  static clang_buggy_consteval auto name() noexcept
  {
    return std::string_view{lit.value};
  }

  value_type value = {setup.init, setup.init};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

template <typename T, static_string lit, range setup>
struct xyz_spinboxes_t
{
  using value_type = xyz_type<T>;
  enum widget
  {
    xyz,
    spinbox
  };
  static clang_buggy_consteval auto range() noexcept
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
  static clang_buggy_consteval auto name() noexcept
  {
    return std::string_view{lit.value};
  }

  value_type value = {setup.init, setup.init, setup.init};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

/// 1D range ///

struct range_slider_range
{
  double min{0.}, max{1.};
  range_slider_value<double> init{0.25, 0.75};
};

template <typename T, static_string lit, range_slider_range setup>
struct range_slider_t
{
  using value_type = range_slider_value<T>;
  enum widget
  {
    hrange_slider
  };
  static clang_buggy_consteval auto range() noexcept { return setup; }
  static clang_buggy_consteval auto name() noexcept
  {
    return std::string_view{lit.value};
  }

  value_type value{setup.init.start, setup.init.end};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

/// RGBA color ///

using color_init = init_range_t<color_type>;

template <static_string lit, color_init setup = color_init{.init = {1., 1., 1., 1.}}>
struct color_chooser
{
  using value_type = color_type;
  enum widget
  {
    color
  };
  static clang_buggy_consteval auto range()
  {
    return init_range_t<value_type>{.init = value_type(setup.init)};
  }
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  value_type value = setup.init;

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

}
