#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/inline.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace halp
{
struct dummy_range { };
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
template <typename T, static_string lit, range setup>
struct slider_t
{
  static clang_buggy_consteval auto range()
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }

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

template <typename T, static_string lit, range setup>
struct hslider_t : slider_t<T, lit, setup>
{
  enum widget
  {
    hslider,
    slider
  };
};

template <typename T, static_string lit, range setup>
struct vslider_t : slider_t<T, lit, setup>
{
  enum widget
  {
    vslider,
    slider
  };
};

template <static_string lit, range setup = default_range<float>>
using hslider_f32 = halp::hslider_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using hslider_i32 = halp::hslider_t<int, lit, setup>;

template <static_string lit, range setup = default_range<float>>
using vslider_f32 = halp::vslider_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using vslider_i32 = halp::vslider_t<int, lit, setup>;

/// Spinbox ///

template <typename T, static_string lit, range setup>
struct spinbox_t : slider_t<T, lit, setup>
{
  enum widget
  {
    spinbox
  };
};

template <static_string lit, range setup = default_range<float>>
using spinbox_f32 = halp::spinbox_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using spinbox_i32 = halp::spinbox_t<int, lit, setup>;

/// Knob ///

template <typename T, static_string lit, auto setup>
struct knob_t
{
  enum widget
  {
    knob
  };
  static clang_buggy_consteval auto range()
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
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

template <static_string lit, auto setup = default_range<float>>
using knob_f32 = halp::knob_t<float, lit, setup>;
template <static_string lit, irange setup = default_irange<int>>
using knob_i32 = halp::knob_t<int, lit, setup>;

// For the case where we want a "float value;" but an integer widget
template <typename T, static_string lit, auto setup>
struct iknob_t
{
  enum widget
  {
    iknob
  };
  static clang_buggy_consteval auto range()
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
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

template <static_string lit, irange setup = default_irange<int>>
using iknob_f32 = halp::knob_t<float, lit, setup>;

// template <static_string lit, long double min, long double max, long double init>
// using knob = halp::knob_t<float, lit, halp::range{min, max, init}>;

/// Time chooser ///
struct ratio { int num{1}, denom{1}; };
template <typename T, static_string lit, range setup>
struct time_chooser_t
{
  enum widget
  {
    time_chooser
  };
  static clang_buggy_consteval auto range()
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }

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

template <static_string lit, range setup = range{.min = 0.001, .max = 5., .init = 0.25}>
using time_chooser = time_chooser_t<float, lit, setup>;

/// Toggle ///

struct toggle_setup
{
  bool init;
  clang_buggy_consteval range to_range() const noexcept
  { return range{0., 1., init ? 1. : 0.}; }
};

template <static_string lit, typename T, auto setup>
struct toggle_t
{
  enum widget
  {
    toggle,
    checkbox
  };
  static clang_buggy_consteval auto range() { return setup; }
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

// Necessary because we have that "toggle" enum member..
template <static_string lit, toggle_setup setup = toggle_setup{false}>
using toggle = toggle_t<lit, bool, setup>;

template <static_string lit, toggle_setup setup = toggle_setup{false}>
using toggle_f32 = toggle_t<lit, float, setup.to_range()>;

/// Button ///
struct impulse
{
};
template <static_string lit>
struct maintained_button_t
{
  enum widget
  {
    button,
    pushbutton
  };
  static clang_buggy_consteval dummy_range range()
  {
    return {};
  }
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
using maintained_button = maintained_button_t<lit>;

struct impulse_type
{
};
template <static_string lit>
struct impulse_button_t
{
  enum widget
  {
    bang,
    impulse
  };
  static clang_buggy_consteval auto range() { return impulse_type{}; }
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  std::optional<impulse_type> value;
  operator bool() const noexcept { return bool(value); }
};

template <static_string lit>
using impulse_button = impulse_button_t<lit>;

/// LineEdit ///
struct lineedit_setup
{
  std::string_view init;
};

template <static_string lit, static_string setup>
struct lineedit_t
{
  lineedit_t() noexcept
      : value{setup.value}
  {
  }

  enum widget
  {
    lineedit,
    textedit,
    text
  };
  static clang_buggy_consteval auto range()
  {
    return lineedit_setup{.init = setup.value};
  }

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  std::string value = setup.value;
  operator std::string&() noexcept { return value; }
  operator const std::string&() const noexcept { return value; }
  auto& operator=(std::string&& t) noexcept
  {
    value = std::move(t);
    return *this;
  }
  auto& operator=(const std::string& t) noexcept
  {
    value = t;
    return *this;
  }
  auto& operator=(std::string_view t) noexcept
  {
    value = t;
    return *this;
  }
};
template <static_string lit, static_string setup>
using lineedit = lineedit_t<lit, setup>;

/// ComboBox / Enum ///

template <typename Enum, static_string lit>
struct enum_t
{
  enum widget
  {
    enumeration,
    list,
    combobox
  };

  static clang_buggy_consteval auto range()
  {
    struct enum_setup
    {
      Enum init{};
    };

    return enum_setup{};
  }

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  Enum value{};
  operator Enum&() noexcept { return value; }
  operator const Enum&() const noexcept { return value; }
  auto& operator=(Enum t) noexcept
  {
    value = t;
    return *this;
  }
};

// { { "foo", 1.5 },  { "bar", 4.0 } }
template <typename T>
struct combo_pair
{
  std::string_view first;
  T second;
};

/* the science isn't there yet...
template<typename T>
using combo_init = combo_pair<T>[];

template <static_string lit, typename ValueType, combo_init in, int idx>
struct combobox_t
{
  using value_type = ValueType;
  enum widget
  {
    enumeration,
    list,
    combobox
  };

  static clang_buggy_consteval auto range()
  {
    struct {
      combo_pair<ValueType> init[std::size(in)];
    } a{in};
    return a;
  }

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  value_type value{};
  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept { value = t; return *this; }
};
*/

/// XY position ///

template <typename T>
struct xy_type
{
  T x, y;

  constexpr xy_type& operator=(T single) noexcept
  {
    x = single;
    y = single;
    return *this;
  }
};

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

template <static_string lit, range setup = default_range<float>>
using xy_pad_f32 = halp::xy_pad_t<float, lit, setup>;


template <typename T>
struct xyz_type
{
  T x, y, z;

  constexpr xyz_type& operator=(T single) noexcept
  {
    x = single;
    y = single;
    z = single;
    return *this;
  }
};


template <typename T, static_string lit, range setup>
struct xyz_spinboxes_t
{
  using value_type = xyz_type<T>;
  enum widget
  {
    xyz, spinbox
  };
  static clang_buggy_consteval auto range() noexcept
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
  static clang_buggy_consteval auto name() noexcept { return std::string_view{lit.value}; }

  value_type value = {setup.init, setup.init, setup.init};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

template <static_string lit, range setup = default_range<float>>
using xyz_spinboxes_f32 = halp::xyz_spinboxes_t<float, lit, setup>;

/// 1D range ///
template <typename T>
struct range_slider_value
{
  T start, end;
};

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
  static clang_buggy_consteval auto name() noexcept { return std::string_view{lit.value}; }

  value_type value{setup.init.start, setup.init.end};

  operator value_type&() noexcept { return value; }
  operator const value_type&() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

template <static_string lit, range_slider_range setup = range_slider_range{}>
using range_slider_f32 = halp::range_slider_t<float, lit, setup>;

/// RGBA color ///
struct color_type
{
  float r, g, b, a;
  constexpr color_type& operator=(float single) noexcept
  {
    r = single;
    g = single;
    b = single;
    a = single;
    return *this;
  }
};

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

/// Bargraph ///

template <typename T, static_string lit, range setup>
struct hbargraph_t : slider_t<T, lit, setup>
{
  enum widget
  {
    hbargraph,
    bargraph
  };
  auto& operator=(T t) noexcept
  {
    this->value = t;
    return *this;
  }
};

template <typename T, static_string lit, range setup>
struct vbargraph_t : slider_t<T, lit, setup>
{
  enum widget
  {
    vbargraph,
    bargraph
  };
  auto& operator=(T t) noexcept
  {
    this->value = t;
    return *this;
  }
};

template <static_string lit, range setup = default_range<float>>
using hbargraph_f32 = halp::hbargraph_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using hbargraph_i32 = halp::hbargraph_t<int, lit, setup>;

template <static_string lit, range setup = default_range<float>>
using vbargraph_f32 = halp::vbargraph_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using vbargraph_i32 = halp::vbargraph_t<int, lit, setup>;

template <typename T>
struct soundfile_view
{
  const T** data{};
  int64_t frames{};
  int32_t channels{};

  // std::fs::path would be great but limits to macOS 10.15+
  std::string_view filename;
};

template <halp::static_string lit, typename T = float>
struct soundfile_port
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  HALP_INLINE_FLATTEN operator soundfile_view<T>&() noexcept { return soundfile; }
  HALP_INLINE_FLATTEN operator const soundfile_view<T>&() const noexcept
  {
    return soundfile;
  }
  HALP_INLINE_FLATTEN operator bool() const noexcept
  {
    return soundfile.data && soundfile.channels > 0 && soundfile.frames > 0;
  }

  HALP_INLINE_FLATTEN std::span<const T> channel(int channel) const noexcept
  {
    return std::span(soundfile.data[channel], soundfile.frames);
  }
  [[nodiscard]] HALP_INLINE_FLATTEN int channels() const noexcept
  {
    return soundfile.channels;
  }
  [[nodiscard]] HALP_INLINE_FLATTEN int64_t frames() const noexcept
  {
    return soundfile.frames;
  }

  HALP_INLINE_FLATTEN const T* operator[](int channel) const noexcept
  {
    return soundfile.data[channel];
  }

  soundfile_view<T> soundfile;
};

}

// Helpers for defining an enumeration without repeating the enumerated members
#define HALP_NUM_ARGS_(_12, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N
#define HALP_NUM_ARGS(...) \
  HALP_NUM_ARGS_(__VA_ARGS__, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define HALP_FOREACH(MACRO, ...) \
  HALP_FOREACH_(HALP_NUM_ARGS(__VA_ARGS__), MACRO, __VA_ARGS__)
#define HALP_FOREACH_(N, M, ...) HALP_FOREACH__(N, M, __VA_ARGS__)
#define HALP_FOREACH__(N, M, ...) HALP_FOREACH_##N(M, __VA_ARGS__)
#define HALP_FOREACH_1(M, A) M(A)
#define HALP_FOREACH_2(M, A, ...) M(A) HALP_FOREACH_1(M, __VA_ARGS__)
#define HALP_FOREACH_3(M, A, ...) M(A) HALP_FOREACH_2(M, __VA_ARGS__)
#define HALP_FOREACH_4(M, A, ...) M(A) HALP_FOREACH_3(M, __VA_ARGS__)
#define HALP_FOREACH_5(M, A, ...) M(A) HALP_FOREACH_4(M, __VA_ARGS__)
#define HALP_FOREACH_6(M, A, ...) M(A) HALP_FOREACH_5(M, __VA_ARGS__)
#define HALP_FOREACH_7(M, A, ...) M(A) HALP_FOREACH_6(M, __VA_ARGS__)
#define HALP_FOREACH_8(M, A, ...) M(A) HALP_FOREACH_7(M, __VA_ARGS__)
#define HALP_FOREACH_9(M, A, ...) M(A) HALP_FOREACH_8(M, __VA_ARGS__)
#define HALP_FOREACH_10(M, A, ...) M(A) HALP_FOREACH_9(M, __VA_ARGS__)
#define HALP_FOREACH_11(M, A, ...) M(A) HALP_FOREACH_10(M, __VA_ARGS__)
#define HALP_FOREACH_12(M, A, ...) M(A) HALP_FOREACH_11(M, __VA_ARGS__)
#define HALP_STRINGIFY_(X) #X
#define HALP_STRINGIFY(X) HALP_STRINGIFY_(X)
#define HALP_STRINGIFY_ALL(...) HALP_FOREACH(HALP_STRINGIFY, __VA_ARGS__)

#define HALP_COMMA(X) X,
#define HALP_COMMA_STRINGIFY(X) HALP_COMMA(HALP_STRINGIFY(X))

#define HALP_STRING_LITERAL_ARRAY(...) HALP_FOREACH(HALP_COMMA_STRINGIFY, __VA_ARGS__)

#define HALP_ENUM_ARRAY_ELEMENT(X) HALP_COMMA({HALP_COMMA_STRINGIFY(X) X})
#define HALP_STRING_LITERAL_ENUM_ARRAY(...) \
  HALP_FOREACH(HALP_ENUM_ARRAY_ELEMENT, __VA_ARGS__)

#define halp__enum(Name, default_v, ...)                 \
  static clang_buggy_consteval auto name()               \
  {                                                      \
    return Name;                                         \
  }                                                      \
  enum enum_type                                         \
  {                                                      \
    __VA_ARGS__                                          \
  } value;                                               \
                                                         \
  enum widget                                            \
  {                                                      \
    enumeration,                                         \
    list,                                                \
    combobox                                             \
  };                                                     \
                                                         \
  struct range                                           \
  {                                                      \
    std::string_view values[HALP_NUM_ARGS(__VA_ARGS__)]{ \
        HALP_STRING_LITERAL_ARRAY(__VA_ARGS__)};         \
    enum_type init = default_v;                          \
  };                                                     \
                                                         \
  operator enum_type&() noexcept                         \
  {                                                      \
    return value;                                        \
  }                                                      \
  operator const enum_type&() const noexcept             \
  {                                                      \
    return value;                                        \
  }                                                      \
  auto& operator=(enum_type t) noexcept                  \
  {                                                      \
    value = t;                                           \
    return *this;                                        \
  }

#define halp__enum_combobox(Name, default_v, ...)                              \
  static clang_buggy_consteval auto name()                                     \
  {                                                                            \
    return Name;                                                               \
  }                                                                            \
  enum enum_type                                                               \
  {                                                                            \
    __VA_ARGS__                                                                \
  } value;                                                                     \
                                                                               \
  enum widget                                                                  \
  {                                                                            \
    combobox                                                                   \
  };                                                                           \
                                                                               \
  struct range                                                                 \
  {                                                                            \
    std::pair<std::string_view, enum_type> values[HALP_NUM_ARGS(__VA_ARGS__)]{ \
        HALP_STRING_LITERAL_ENUM_ARRAY(__VA_ARGS__)};                          \
    enum_type init = default_v;                                                \
  };                                                                           \
                                                                               \
  operator enum_type&() noexcept                                               \
  {                                                                            \
    return value;                                                              \
  }                                                                            \
  operator const enum_type&() const noexcept                                   \
  {                                                                            \
    return value;                                                              \
  }                                                                            \
  auto& operator=(enum_type t) noexcept                                        \
  {                                                                            \
    value = t;                                                                 \
    return *this;                                                              \
  }
