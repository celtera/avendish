#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/aggregates.hpp>
#include <avnd/common/dummy.hpp>

#include <string_view>
#include <type_traits>

namespace avnd
{
#define if_possible(action)          \
  if constexpr(requires { action; }) \
  {                                  \
    action;                          \
  }

#define value_if_possible(A, X, B) \
  []() consteval                   \
  {                                \
    if constexpr(requires { A; })  \
      return A;                    \
    else                           \
      return B;                    \
  }                                \
  ()

#if defined(_MSC_VER)
#define assign_if_assignable(dst, src)                             \
  do                                                               \
    if constexpr(requires { src; })                                \
    {                                                              \
      using val_type = std::add_lvalue_reference_t<decltype(dst)>; \
      using ret_type = decltype(src);                              \
      if constexpr(std::is_assignable_v<val_type, ret_type>)       \
        dst = src;                                                 \
    }                                                              \
  while(0)
#else
#define assign_if_assignable(dst, src) if_possible(dst = src);
#endif

// Very generic concepts
template <typename T>
concept iterable_ish = requires(const T& t) {
  std::size(t);
  std::begin(t);
  std::end(t);
};
template <typename T>
concept span_ish = iterable_ish<T> && requires(const T& t) { t[0]; };
template <typename T>
concept list_ish = requires(T t) {
  t.push_back({});
  // t.reserve(1); // many containers don't have it
  t.resize(1);
  t.clear();
  // t.data(); // did you know? std::vector<bool> does not have it
};
template <typename T>
concept vector_ish = span_ish<T> && list_ish<T>;

template <typename T>
concept set_ish = iterable_ish<T> && requires(T t) {
  sizeof(typename T::key_type);
  sizeof(typename T::value_type);
  t.insert(typename T::key_type{});
  t.find(typename T::key_type{});
  t.size();
  t.clear();
} && !requires(T t) { sizeof(typename T::mapped_type); };

template <typename T>
concept map_ish = iterable_ish<T> && requires(T t) {
  sizeof(typename T::key_type);
  sizeof(typename T::mapped_type);
  sizeof(typename T::value_type);
  t.insert(typename T::value_type{});
  t.find(typename T::key_type{});
  t[typename T::key_type{}];
  t.size();
  t.clear();
};

template <typename T, typename V>
concept vector_v_ish = span_ish<T> && requires(T t) {
  t.push_back(V{});
  t.resize(1);
  t.reserve(1);
  t.clear();
  t.data();
  { t[1] } -> std::convertible_to<V>;
  t[1] = std::declval<V>();
};

template <typename T, typename V>
concept vector_v_strict = vector_ish<T> && std::is_same_v<typename T::value_type, V>;

template <typename T>
concept tuple_ish = requires(const T& t) {
  std::tuple_size<T>::value;
  typename std::tuple_element_t<0, T>;
  std::get<0>(t);
};

template <typename T>
concept variant_ish = requires(T t) {
  t.index();
  t.valueless_by_exception();
};

template <typename T>
concept pair_ish = tuple_ish<T> && requires(T t) {
  t.first;
  t.second;
} && (std::tuple_size_v<T> == 2);

template <typename T>
concept optional_ish = requires(T t) {
                         bool(t);
                         t.value();
                         t.reset();
                         *t;
                       };

template <typename T, std::size_t N>
concept c_array_ish = span_ish<T> && (std::extent_v<T, 0> >= N);
template <typename T, std::size_t N>
concept cpp_tuple_ish
    = tuple_ish<T> && requires { std::tuple_size_v<std::remove_cvref_t<T>> == N; };
template <typename T, std::size_t N>
concept cpp_array_ish = span_ish<T> && !vector_ish<T> && cpp_tuple_ish<T, N>;

template <typename T, std::size_t N>
concept array_ish = span_ish<T> && (c_array_ish<T, N> || cpp_array_ish<T, N>);

template <typename T>
concept static_array_ish = span_ish<T> && tuple_ish<T>;

template <typename T>
concept bitset_ish = requires(T t) {
                       t.size();

                       t.test(123);

                       t.set();
                       t.set(123);

                       t.reset();
                       t.reset(123);
                     };

template <typename T>
concept pointer = std::is_pointer_v<std::decay_t<T>>;

template <typename T>
concept string_ish = span_ish<T> && requires(T t, std::string_view v) { v = t; };

template <typename T>
concept int_ish = std::is_same_v<std::decay_t<T>, signed int>
                  || std::is_same_v<std::remove_reference_t<T>, signed short>
                  || std::is_same_v<std::decay_t<T>, signed long>
                  || std::is_same_v<std::decay_t<T>, signed long long>
                  || std::is_same_v<std::decay_t<T>, unsigned int>
                  || std::is_same_v<std::decay_t<T>, unsigned short>
                  || std::is_same_v<std::decay_t<T>, unsigned long>
                  || std::is_same_v<std::decay_t<T>, unsigned long long>;

template <typename T>
concept fp_ish = std::is_floating_point_v<std::decay_t<T>>;

template <typename T>
concept bool_ish = std::is_same_v<std::decay_t<T>, bool>;

template <typename T>
struct is_type_list : std::false_type
{
};
template <template <typename...> typename T, typename... Args>
struct is_type_list<T<Args...>> : std::true_type
{
};

template <typename T>
concept type_list = is_type_list<std::remove_const_t<T>>::value;

template <typename T>
concept enum_ish = std::is_enum_v<std::decay_t<T>>;

template<typename T>
concept dict_ish = map_ish<T> && string_ish<typename T::key_type>;

/**
 * A macro to define a set of concepts & type-checks to devise if
 * in the expression T::foo, foo is a type or a value.
 *
 * Will define:
 * concept foo_is_value;
 * concept foo_is_type;
 * concept has_foo;
 *
 *
 * Used for instance for inputs and outputs introspection.
 * C++2y: we'd like to have the token "inputs" itself be generic
 */
#define type_or_value_qualification(Name)                                        \
  template <typename T>                                                          \
  concept Name##_is_value = requires(T t) { decltype(std::decay_t<T>::Name){}; } \
                            || requires(T t) { &std::declval<T&>().Name; };      \
                                                                                 \
  template <typename T>                                                          \
  concept Name##_is_type                                                         \
      = requires(T t) { !std::is_void_v<typename std::decay_t<T>::Name>; };      \
                                                                                 \
  template <typename T>                                                          \
  concept has_##Name = Name##_is_type<T> || Name##_is_value<T>;

/**
 * A macro that leverages the previous macro to define helper types,
 * which will allow to get access to the information of a named field
 * no matter if it's a type or a value.
 *
 * e.g. the following types allow to get the type of the
 * "inputs" / "outputs" member whether they are types
 * or values:
 *
 * struct foo {
 *   struct inputs {
 *     ..
 *   };
 * };
 *
 * struct bar {
 *   struct {
 *     ..
 *   } inputs;
 * };
 *
 * inputs_type<foo>::type will yield the type of "inputs".
 * inputs_type<bar>::type will yield the type of "inputs".
 *
 */
#define type_or_value_reflection(Name)                                             \
  template <typename T>                                                            \
  struct Name##_type                                                               \
  {                                                                                \
    using type = dummy;                                                            \
    using tuple = tpl::tuple<>;                                                    \
    static constexpr const auto size = 0;                                          \
  };                                                                               \
                                                                                   \
  template <Name##_is_type T>                                                      \
  struct Name##_type<T>                                                            \
  {                                                                                \
    using type = typename std::decay_t<T>::Name;                                   \
    using tuple = avnd::as_typelist<type>;                                         \
    static constexpr const auto size = pfr::tuple_size_v<type>;                    \
  };                                                                               \
                                                                                   \
  template <Name##_is_value T>                                                     \
  struct Name##_type<T>                                                            \
  {                                                                                \
    using type                                                                     \
        = std::remove_reference_t<decltype(std::declval<std::decay_t<T>>().Name)>; \
    using tuple = avnd::as_typelist<type>;                                         \
    static constexpr const auto size = pfr::tuple_size_v<type>;                    \
  };
}

#define type_or_value_accessors(Name)                 \
  template <typename T>                               \
    requires(Name##_is_type<T> || Name##_is_value<T>) \
  auto get_##Name(T&& t)->decltype(auto)              \
  {                                                   \
    if constexpr(Name##_is_type<T>)                   \
      return typename std::decay_t<T>::Name{};        \
    else if constexpr(Name##_is_value<T>)             \
      return std::forward<T>(t).Name;                 \
  }
