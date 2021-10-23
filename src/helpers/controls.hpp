#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include <string_view>

namespace avnd
{

template <std::size_t N>
struct static_string
{
  consteval static_string(const char (&str)[N]) noexcept
  {
    std::copy_n(str, N, value);
  }

  char value[N];
};

template <typename T>
struct range_t
{
  T min, max, init;
};
using range = range_t<long double>;
template <typename T>
constexpr auto default_range = range{0., 1., 0.5};
template <>
constexpr auto default_range<int> = range{0., 127., 64.};

template <typename T, static_string lit, range setup>
struct slider_t
{
  enum widget
  {
    slider
  };
  static consteval auto control()
  {
    return range_t<T>{
        .min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
  static consteval auto name() { return std::string_view{lit.value}; }

  T value = setup.init;
};

template <typename T, static_string lit, range setup>
struct knob_t
{
  enum widget
  {
    knob
  };
  static consteval auto control()
  {
    return range_t<T>{
        .min = T(setup.min), .max = T(setup.max), .init = T(setup.init)};
  }
  static consteval auto name() { return std::string_view{lit.value}; }

  T value = setup.init;
};

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
  static consteval auto control() { return setup; }
  static consteval auto name() { return std::string_view{lit.value}; }

  bool value = setup.init;
};

// Necessary because we have that "toggle" enum member..
template <static_string lit, toggle_setup setup>
using toggle = toggle_t<lit, setup>;

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
  static consteval auto control()
  {
    return lineedit_setup{.init = setup.value};
  }

  static consteval auto name() { return std::string_view{lit.value}; }

  std::string value = setup.value;
};
template <static_string lit, static_string setup>
using lineedit = lineedit_t<lit, setup>;

template <typename Enum, static_string lit>
struct enum_t
{
  enum widget
  {
    list,
    combobox
  };
  static consteval auto control()
  {
    struct enum_setup
    {
      Enum init{};
    };

    return enum_setup{};
  }

  static consteval auto name() { return std::string_view{lit.value}; }

  Enum value{};
};

template <static_string lit, range setup = default_range<float>>
using slider_f32 = avnd::slider_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using slider_i32 = avnd::slider_t<int, lit, setup>;

template <static_string lit, range setup = default_range<float>>
using knob_f32 = avnd::knob_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using knob_i32 = avnd::knob_t<int, lit, setup>;

template <static_string lit, auto M>
struct func_ref
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static consteval auto func() { return M; }
};

}

// Helpers for enums
#define AVND_NUM_ARGS_(_10, _9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N
#define AVND_NUM_ARGS(...) \
  AVND_NUM_ARGS_(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
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

#define STRING_LITERAL_ARRAY(...) \
  AVND_FOREACH(COMMA_AVND_STRINGIFY, __VA_ARGS__)

#define avnd__enum(Name, default_v, ...)                                      \
  struct                                                                      \
  {                                                                           \
    enum enum_type                                                            \
    {                                                                         \
      __VA_ARGS__                                                             \
    } value;                                                                  \
    static consteval auto name() { return Name; }                             \
    static consteval auto control()                                           \
    {                                                                         \
      struct                                                                  \
      {                                                                       \
        enum_type init = default_v;                                           \
      } ctl;                                                                  \
      return ctl;                                                             \
    }                                                                         \
    static consteval std::array<std::string_view, AVND_NUM_ARGS(__VA_ARGS__)> \
    choices()                                                                 \
    {                                                                         \
      return {STRING_LITERAL_ARRAY(__VA_ARGS__)};                             \
    }                                                                         \
  }
