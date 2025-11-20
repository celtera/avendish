#pragma once
#include <halp/inline.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>
#include <halp/value_types.hpp>

#include <string_view>
#include <type_traits>
#if __has_include(<magic_enum.hpp>)
#include <magic_enum.hpp>
#elif __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#error magic_enum is required
#endif

#include <halp/modules.hpp>
HALP_MODULE_EXPORT
namespace halp
{
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

#if MAGIC_ENUM_SUPPORTED
  // FIXME eventually we would like to be able to handle
  // enum Foo { first = 120, last = -840 };
  // but so far the code expects contiguousness and starting at 0 in many places
  static consteval bool enum_is_contiguous() noexcept
  {
    constexpr auto values = magic_enum::enum_values<Enum>();
    int i = 0;
    for(auto val : values)
    {
      if(magic_enum::enum_underlying(val) != i)
        return false;
      i++;
    }
    return true;
  }
  static_assert(enum_is_contiguous());
#endif

  struct range
  {
#if MAGIC_ENUM_SUPPORTED
    decltype(magic_enum::enum_names<Enum>()) values = magic_enum::enum_names<Enum>();
#endif
    Enum init{};
  };

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  Enum value{};
  operator Enum&() noexcept { return value; }
  operator const Enum&() const noexcept { return value; }
  auto& operator=(Enum t) noexcept
  {
    value = t;
    return *this;
  }
  auto& operator=(std::string_view t) noexcept
  {
#if MAGIC_ENUM_SUPPORTED
    if(auto res = magic_enum::enum_cast<Enum>(t))
      value = *res;
#endif
    return *this;
  }
};

template <typename Enum, static_string lit>
struct string_enum_t
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
#if MAGIC_ENUM_SUPPORTED
      decltype(magic_enum::enum_names<Enum>()) values = magic_enum::enum_names<Enum>();
#endif
      Enum init{};
    };

    return enum_setup{};
  }

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  std::string value{};
  operator std::string&() noexcept { return value; }
  operator const std::string&() const noexcept { return value; }
  auto& operator=(std::string t) noexcept
  {
    value = std::move(t);
    return *this;
  }
  auto& operator=(Enum t) noexcept
  {
#if MAGIC_ENUM_SUPPORTED
    value = magic_enum::enum_name(t);
#endif
    return *this;
  }
  auto& operator=(std::integral auto t) noexcept
  {
#if MAGIC_ENUM_SUPPORTED
    value = magic_enum::enum_name(static_cast<Enum>(t));
#endif
    return *this;
  }
};

/* the science isn't there yet...
 * last try: https://gcc.godbolt.org/z/4Y8hvcqch 
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

// Helpers for defining an enumeration without repeating the enumerated members
#define HALP_NUM_ARGS_(                                                               \
    _35, _34, _33, _32, _31, _30, _29, _28, _27, _26, _25, _24, _23, _22, _21, _20,   \
    _19, _18, _17, _16, _15, _14, _13, _12, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, \
    _1, N, ...)                                                                       \
  N
#define HALP_NUM_ARGS(...)                                                             \
  HALP_NUM_ARGS_(                                                                      \
      __VA_ARGS__, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, \
      18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
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
#define HALP_FOREACH_13(M, A, ...) M(A) HALP_FOREACH_12(M, __VA_ARGS__)
#define HALP_FOREACH_14(M, A, ...) M(A) HALP_FOREACH_13(M, __VA_ARGS__)
#define HALP_FOREACH_15(M, A, ...) M(A) HALP_FOREACH_14(M, __VA_ARGS__)
#define HALP_FOREACH_16(M, A, ...) M(A) HALP_FOREACH_15(M, __VA_ARGS__)
#define HALP_FOREACH_17(M, A, ...) M(A) HALP_FOREACH_16(M, __VA_ARGS__)
#define HALP_FOREACH_18(M, A, ...) M(A) HALP_FOREACH_17(M, __VA_ARGS__)
#define HALP_FOREACH_19(M, A, ...) M(A) HALP_FOREACH_18(M, __VA_ARGS__)
#define HALP_FOREACH_20(M, A, ...) M(A) HALP_FOREACH_19(M, __VA_ARGS__)
#define HALP_FOREACH_21(M, A, ...) M(A) HALP_FOREACH_20(M, __VA_ARGS__)
#define HALP_FOREACH_22(M, A, ...) M(A) HALP_FOREACH_21(M, __VA_ARGS__)
#define HALP_FOREACH_23(M, A, ...) M(A) HALP_FOREACH_22(M, __VA_ARGS__)
#define HALP_FOREACH_24(M, A, ...) M(A) HALP_FOREACH_23(M, __VA_ARGS__)
#define HALP_FOREACH_25(M, A, ...) M(A) HALP_FOREACH_24(M, __VA_ARGS__)
#define HALP_FOREACH_26(M, A, ...) M(A) HALP_FOREACH_25(M, __VA_ARGS__)
#define HALP_FOREACH_27(M, A, ...) M(A) HALP_FOREACH_26(M, __VA_ARGS__)
#define HALP_FOREACH_28(M, A, ...) M(A) HALP_FOREACH_27(M, __VA_ARGS__)
#define HALP_FOREACH_29(M, A, ...) M(A) HALP_FOREACH_28(M, __VA_ARGS__)
#define HALP_FOREACH_30(M, A, ...) M(A) HALP_FOREACH_29(M, __VA_ARGS__)
#define HALP_FOREACH_31(M, A, ...) M(A) HALP_FOREACH_30(M, __VA_ARGS__)
#define HALP_FOREACH_32(M, A, ...) M(A) HALP_FOREACH_31(M, __VA_ARGS__)
#define HALP_FOREACH_33(M, A, ...) M(A) HALP_FOREACH_32(M, __VA_ARGS__)
#define HALP_FOREACH_34(M, A, ...) M(A) HALP_FOREACH_33(M, __VA_ARGS__)
#define HALP_FOREACH_35(M, A, ...) M(A) HALP_FOREACH_34(M, __VA_ARGS__)
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

}
