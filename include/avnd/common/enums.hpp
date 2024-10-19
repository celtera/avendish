#pragma once
#include <avnd/common/tag.hpp>

/**
 * This file defines a nifty set of utilities: AVND_ENUM_MATCHER and AVND_TAG_MATCHER.
 *
 * AVND_ENUM_MATCHER:
 * Usage:
 *
 * Given a target enum:
 * enum my_enum { foo, bar, baz };
 *
 * And a set of words that should match to each enumerator:
 * for instance, foo or Foo for the first enumerator, bar or Bar for the second, etc.
 * in order to introspect a class that may define a custom enum:
 *
 * ```
 * auto matcher = AVND_ENUM_MATCHER(
 *     (my_enum::foo, foo, Foo),
 *     (my_enum::bar, bar, Bar)
 *     );
 * ```
 *
 * Then:
 *
 * ```
 * auto res = matcher(
 *     a_value_of_unknown_enum_type  //< input
 *   , my_enum::foo                  //< default value when input not found
 * );
 * ```
 *
 * `res` will be the corresponding enumerator in `my_enum`.
 *
 * e.g. one can do
 *
 * ```
 * enum { Foo, Bar } x = Foo;
 * ```
 *
 * and convert that to a my_enum::foo or my_enum::bar.
 *
 *
 * AVND_TAG_MATCHER: the same, but looks for tags in structs:
 * struct Foo {
 *   enum { ui, test };
 * };
 *
 * should tell us if there is an "ui" tag in Foo.
 *
 *
 * AVND_ENUM_OR_TAG_MATCHER: combines both the static and dynamic cases
 *
 * AVND_*_CONVERTER: does the reverse: it tries to convert to a matching case in the
 * target enum
 *
 */

#define AVND_NUM_ARGS_(_12, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N
#define AVND_NUM_ARGS(...) \
  AVND_NUM_ARGS_(__VA_ARGS__, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
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
#define AVND_FOREACH_9(M, A, ...) M(A) AVND_FOREACH_8(M, __VA_ARGS__)
#define AVND_FOREACH_10(M, A, ...) M(A) AVND_FOREACH_9(M, __VA_ARGS__)
#define AVND_FOREACH_11(M, A, ...) M(A) AVND_FOREACH_10(M, __VA_ARGS__)
#define AVND_FOREACH_12(M, A, ...) M(A) AVND_FOREACH_11(M, __VA_ARGS__)

#define AVND_FOREACHSUB1(MACRO, ...) \
  AVND_FOREACHSUB1_(AVND_NUM_ARGS(__VA_ARGS__), MACRO, __VA_ARGS__)
#define AVND_FOREACHSUB1_(N, M, ...) AVND_FOREACHSUB1__(N, M, __VA_ARGS__)
#define AVND_FOREACHSUB1__(N, M, ...) AVND_FOREACHSUB1_##N(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_1(M, A) M(A)
#define AVND_FOREACHSUB1_2(M, A, ...) M(A) AVND_FOREACHSUB1_1(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_3(M, A, ...) M(A) AVND_FOREACHSUB1_2(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_4(M, A, ...) M(A) AVND_FOREACHSUB1_3(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_5(M, A, ...) M(A) AVND_FOREACHSUB1_4(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_6(M, A, ...) M(A) AVND_FOREACHSUB1_5(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_7(M, A, ...) M(A) AVND_FOREACHSUB1_6(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_8(M, A, ...) M(A) AVND_FOREACHSUB1_7(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_9(M, A, ...) M(A) AVND_FOREACHSUB1_8(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_10(M, A, ...) M(A) AVND_FOREACHSUB1_9(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_11(M, A, ...) M(A) AVND_FOREACHSUB1_10(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_12(M, A, ...) M(A) AVND_FOREACHSUB1_11(M, __VA_ARGS__)

#define AVND_ENUM_MATCHER_IMPL_IMPL(N)           \
  if constexpr(requires { decltype(value)::N; }) \
  {                                              \
    ok = ok || (value == (decltype(value)::N));  \
  }

#define AVND_ENUM_MATCHER_IMPL2(id, ...)                        \
  {                                                             \
    bool ok = false;                                            \
    AVND_FOREACHSUB1(AVND_ENUM_MATCHER_IMPL_IMPL, __VA_ARGS__); \
    if(ok)                                                      \
      return id;                                                \
  }

#define AVND_ENUM_MATCHER_IMPL(...) AVND_ENUM_MATCHER_IMPL2 __VA_ARGS__

#define AVND_MATCH_ALL_ENUMS(...) AVND_FOREACH(AVND_ENUM_MATCHER_IMPL, __VA_ARGS__)

#define AVND_ENUM_MATCHER(...)                                                     \
  [](auto value, auto defaultvalue) constexpr noexcept -> decltype(defaultvalue) { \
    AVND_MATCH_ALL_ENUMS(__VA_ARGS__);                                             \
    return defaultvalue;                                                           \
  };

#define AVND_TAG_MATCHER_IMPL_IMPL(N) || requires { TMatcher::N; }

#define AVND_TAG_MATCHER_IMPL2(id, ...)                                       \
  {                                                                           \
    if constexpr(0 AVND_FOREACHSUB1(AVND_TAG_MATCHER_IMPL_IMPL, __VA_ARGS__)) \
      return id;                                                              \
  }

#define AVND_TAG_MATCHER_IMPL(...) AVND_TAG_MATCHER_IMPL2 __VA_ARGS__

#define AVND_MATCH_ALL_TAGS(...) AVND_FOREACH(AVND_TAG_MATCHER_IMPL, __VA_ARGS__)
#define AVND_TAG_MATCHER(...)                                           \
  []<typename TMatcher>(                                                \
      TMatcher& container,                                              \
      auto defaultvalue) constexpr noexcept -> decltype(defaultvalue) { \
    AVND_MATCH_ALL_TAGS(__VA_ARGS__);                                   \
    return defaultvalue;                                                \
  };

#define AVND_ENUM_OR_TAG_MATCHER(member_name, ...)                   \
  [](auto& container, auto defaultvalue) -> decltype(defaultvalue) { \
    if constexpr(requires { container.member_name; })                \
    {                                                                \
      static constexpr auto m = AVND_ENUM_MATCHER(__VA_ARGS__);             \
      return m(container.member_name, defaultvalue);                 \
    }                                                                \
    else                                                             \
    {                                                                \
      static constexpr auto m = AVND_TAG_MATCHER(__VA_ARGS__);              \
      return m(container, defaultvalue);                             \
    }                                                                \
  };

#define AVND_ENUM_CONVERTER_IMPL_IMPL(N)                \
  if constexpr(requires { decltype(defaultvalue)::N; }) \
  {                                                     \
    return decltype(defaultvalue)::N;                   \
  }

#define AVND_ENUM_CONVERTER_IMPL2(id, ...)                        \
  case id: {                                                      \
    AVND_FOREACHSUB1(AVND_ENUM_CONVERTER_IMPL_IMPL, __VA_ARGS__); \
    break;                                                        \
  }

#define AVND_ENUM_CONVERTER_IMPL(...) AVND_ENUM_CONVERTER_IMPL2 __VA_ARGS__

#define AVND_CONVERT_ALL_ENUMS(...) AVND_FOREACH(AVND_ENUM_CONVERTER_IMPL, __VA_ARGS__)

#define AVND_ENUM_CONVERTER(...)                                                   \
  [](auto value, auto defaultvalue) constexpr noexcept -> decltype(defaultvalue) { \
    switch(value)                                                                  \
    {                                                                              \
      AVND_CONVERT_ALL_ENUMS(__VA_ARGS__);                                         \
    }                                                                              \
    return defaultvalue;                                                           \
  };
