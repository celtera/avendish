#pragma once
#include <avnd/helpers/static_string.hpp>

#include <array>
#include <cstddef>
#include <string>

#include <string_view>

namespace avnd
{

template <typename T>
struct range_t
{
  T min, max, init;
};
using range = range_t<long double>;
template <typename T>
inline constexpr auto default_range = range{0., 1., 0.5};
template <>
inline constexpr auto default_range<int> = range{0., 127., 64.};

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
  static consteval auto range()
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }

  static consteval auto name() { return std::string_view{lit.value}; }

  T value = setup.init;

  operator T&() noexcept { return value; }
  operator T() const noexcept { return value; }
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
using hslider_f32 = avnd::hslider_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using hslider_i32 = avnd::hslider_t<int, lit, setup>;

template <static_string lit, range setup = default_range<float>>
using vslider_f32 = avnd::vslider_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using vslider_i32 = avnd::vslider_t<int, lit, setup>;

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
using spinbox_f32 = avnd::spinbox_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using spinbox_i32 = avnd::spinbox_t<int, lit, setup>;

/// Knob ///

template <typename T, static_string lit, range setup>
struct knob_t
{
  enum widget
  {
    knob
  };
  static consteval auto range()
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
  static consteval auto name() { return std::string_view{lit.value}; }

  T value = setup.init;

  operator T&() noexcept { return value; }
  operator T() const noexcept { return value; }
  auto& operator=(T t) noexcept
  {
    value = t;
    return *this;
  }
};

template <static_string lit, range setup = default_range<float>>
using knob_f32 = avnd::knob_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using knob_i32 = avnd::knob_t<int, lit, setup>;

/// Toggle ///

struct toggle_setup
{
  bool init;
};

template <static_string lit, toggle_setup setup>
struct toggle_t
{
  enum widget
  {
    toggle,
    checkbox
  };
  static consteval auto range() { return setup; }
  static consteval auto name() { return std::string_view{lit.value}; }

  bool value = setup.init;

  operator bool&() noexcept { return value; }
  operator bool() const noexcept { return value; }
  auto& operator=(bool t) noexcept
  {
    value = t;
    return *this;
  }
};

// Necessary because we have that "toggle" enum member..
template <static_string lit, toggle_setup setup>
using toggle = toggle_t<lit, setup>;

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
    pushbutton,
    bang
  };
  static consteval auto range()
  {
    struct
    {
    } dummy;
    return dummy;
  }
  static consteval auto name() { return std::string_view{lit.value}; }

  bool value = false;
  operator bool&() noexcept { return value; }
  operator bool() const noexcept { return value; }
  auto& operator=(bool t) noexcept
  {
    value = t;
    return *this;
  }
};

template <static_string lit>
using maintained_button = maintained_button_t<lit>;

template <static_string lit>
struct impulse_button_t
{
  enum widget
  {
    bang,
    button,
    pushbutton
  };
  static consteval auto range()
  {
    struct
    {
    } dummy;
    return dummy;
  }
  static consteval auto name() { return std::string_view{lit.value}; }

  bool value = false;
  operator bool&() noexcept { return value; }
  operator bool() const noexcept { return value; }
  auto& operator=(bool t) noexcept
  {
    value = t;
    return *this;
  }
};

template <static_string lit>
using impulse_button = maintained_button_t<lit>;

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
  static consteval auto range() { return lineedit_setup{.init = setup.value}; }

  static consteval auto name() { return std::string_view{lit.value}; }

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

  static consteval auto range()
  {
    struct enum_setup
    {
      Enum init{};
    };

    return enum_setup{};
  }

  static consteval auto name() { return std::string_view{lit.value}; }

  Enum value{};
  operator Enum&() noexcept { return value; }
  operator Enum() const noexcept { return value; }
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

  static consteval auto range()
  {
    struct {
      combo_pair<ValueType> init[std::size(in)];
    } a{in};
    return a;
  }

  static consteval auto name() { return std::string_view{lit.value}; }

  value_type value{};
  operator value_type&() noexcept { return value; }
  operator value_type() const noexcept { return value; }
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
  static consteval auto range()
  {
    return range_t<T>{.min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
  static consteval auto name() { return std::string_view{lit.value}; }

  value_type value = {setup.init, setup.init};

  operator value_type&() noexcept { return value; }
  operator value_type() const noexcept { return value; }
  auto& operator=(value_type t) noexcept
  {
    value = t;
    return *this;
  }
};

template <static_string lit, range setup = default_range<float>>
using xy_pad_f32 = avnd::xy_pad_t<float, lit, setup>;

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
  static consteval auto range()
  {
    return init_range_t<value_type>{.init = value_type(setup.init)};
  }
  static consteval auto name() { return std::string_view{lit.value}; }

  value_type value = setup.init;

  operator value_type&() noexcept { return value; }
  operator value_type() const noexcept { return value; }
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
using hbargraph_f32 = avnd::hbargraph_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using hbargraph_i32 = avnd::hbargraph_t<int, lit, setup>;

template <static_string lit, range setup = default_range<float>>
using vbargraph_f32 = avnd::vbargraph_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using vbargraph_i32 = avnd::vbargraph_t<int, lit, setup>;

}

// Helpers for defining an enumeration without repeating the enumerated members
#define AVND_NUM_ARGS_(_10, _9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N
#define AVND_NUM_ARGS(...) AVND_NUM_ARGS_(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define AVND_FOREACH(MACRO, ...) \
  AVND_FOREACH_(AVND_NUM_ARGS(__VA_ARGS__), MACRO, __VA_ARGS__)
#define AVND_FOREACH_(N, M, ...) AVND_FOREACH__(N, M, __VA_ARGS__)
#define AVND_FOREACH__(N, M, ...) AVND_FOREACH_##N(M, __VA_ARGS__)
#define AVND_FOREACH_1(M, A) M(A)
#define AVND_FOREACH_2(M, A, ...) M(A) AVND_FOREACH_1(M, __VA_ARGS__)
#define AVND_FOREACH_3(M, A, ...) M(A) AVND_FOREACH_2(M, __VA_ARGS__)
#define AVND_FOREACH_4(M, A, ...) M(A) AVND_FOREACH_3(M, __VA_ARGS__)
#define AVND_FOREACH_5(M, A, ...) M(A) AVND_FOREACH_4(M, __VA_ARGS__)
#define AVND_FOREACH_6(M, A, ...) M(A) AVND_FOREACH_5(M, __VA_ARGS__)
#define AVND_FOREACH_7(M, A, ...) M(A) AVND_FOREACH_6(M, __VA_ARGS__)
#define AVND_FOREACH_8(M, A, ...) M(A) AVND_FOREACH_7(M, __VA_ARGS__)
#define AVND_STRINGIFY_(X) #X
#define AVND_STRINGIFY(X) AVND_STRINGIFY_(X)
#define AVND_STRINGIFY_ALL(...) AVND_FOREACH(AVND_STRINGIFY, __VA_ARGS__)

#define COMMA(X) X,
#define COMMA_AVND_STRINGIFY(X) COMMA(AVND_STRINGIFY(X))

#define STRING_LITERAL_ARRAY(...) AVND_FOREACH(COMMA_AVND_STRINGIFY, __VA_ARGS__)

#define avnd__enum(Name, default_v, ...)                                                \
  struct                                                                                \
  {                                                                                     \
    enum enum_type                                                                      \
    {                                                                                   \
      __VA_ARGS__                                                                       \
    } value;                                                                            \
                                                                                        \
    enum widget                                                                         \
    {                                                                                   \
      enumeration,                                                                      \
      list,                                                                             \
      combobox                                                                          \
    };                                                                                  \
                                                                                        \
    static consteval std::array<std::string_view, AVND_NUM_ARGS(__VA_ARGS__)> choices() \
    {                                                                                   \
      return {STRING_LITERAL_ARRAY(__VA_ARGS__)};                                       \
    }                                                                                   \
                                                                                        \
    static consteval auto name()                                                        \
    {                                                                                   \
      return Name;                                                                      \
    }                                                                                   \
    static consteval auto range()                                                       \
    {                                                                                   \
      struct                                                                            \
      {                                                                                 \
        enum_type init = default_v;                                                     \
      } ctl;                                                                            \
      return ctl;                                                                       \
    }                                                                                   \
                                                                                        \
    operator enum_type&() noexcept                                                      \
    {                                                                                   \
      return value;                                                                     \
    }                                                                                   \
    operator enum_type() const noexcept                                                 \
    {                                                                                   \
      return value;                                                                     \
    }                                                                                   \
    auto& operator=(enum_type t) noexcept                                               \
    {                                                                                   \
      value = t;                                                                        \
      return *this;                                                                     \
    }                                                                                   \
  }
