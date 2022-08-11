#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/function_reflection.hpp>
#include <avnd/concepts/generic.hpp>

#include <vector>

#include <string_view>

namespace avnd
{
/***
 * auto func() { return &some_free_function; }
 * auto func() { return &Foo::some_member; }
 * etc..
 */
template <typename T>
concept stateless_message = requires
{
  typename avnd::function_reflection<T::func()>::return_type;
};

/***
 * auto func() { return [] { ... }; }
 */
template <typename T>
concept stateful_message = requires
{
  typename avnd::function_reflection_o<decltype(T::func())>::return_type;
};

/***
 * std::function<void(...)> func() { return ...; }
 */
template <typename T>
concept stdfunc_message = requires
{
  typename avnd::function_reflection_t<decltype(T::func())>::return_type;
};

/***
 * void operator()(...) { }
 */
template <typename T>
concept function_object_message = requires
{
  typename avnd::function_reflection<&T::operator()>::return_type;
};

template <typename Node, typename T>
concept variadic_function_object_message = requires(T t)
{
  t(std::vector<int>{});
}
|| requires(Node n, T t)
{
  t(n, std::vector<int>{});
};

template <typename T>
concept reflectable_message = stateless_message<T> || stateful_message<
    T> || stdfunc_message<T> || function_object_message<T>;

template <typename T>
concept message = reflectable_message<T> && requires(T t)
{
  {
    t.name()
    } -> string_ish;
};

template <typename T, typename N>
concept unreflectable_message = (!reflectable_message<T>)&&requires(T t)
{
  t.name();
}
&&(requires(T t) { t.func(); } || variadic_function_object_message<N, T>);

type_or_value_qualification(messages)
type_or_value_reflection(messages)

template <typename M>
consteval auto message_function_reflection()
{
  if constexpr(stateless_message<M>)
  {
    return avnd::function_reflection<M::func()>{};
  }
  else if constexpr(stateful_message<M>)
  {
    return avnd::function_reflection_o<decltype(M::func())>{};
  }
  else if constexpr(stdfunc_message<M>)
  {
    return avnd::function_reflection_t<decltype(M::func())>{};
  }
  else if constexpr(function_object_message<M>)
  {
    return avnd::function_reflection<&M::operator()>{};
  }
  else
  {
    return;
  }
}

template <typename M>
consteval auto message_get_func()
{
  if constexpr(stateless_message<M>)
  {
    return M::func();
  }
  else if constexpr(stateful_message<M>)
  {
    return M::func();
  }
  else if constexpr(stdfunc_message<M>)
  {
    return M::func();
  }
  else if constexpr(function_object_message<M>)
  {
    return &M::operator();
  }
  else
  {
    return;
  }
}

template <typename M>
using message_reflection = decltype(message_function_reflection<M>());

template <typename M>
using first_message_argument
    = boost::mp11::mp_first<typename message_reflection<M>::arguments>;
template <typename M>
using second_message_argument
    = boost::mp11::mp_second<typename message_reflection<M>::arguments>;
template <typename M>
using third_message_argument
    = boost::mp11::mp_third<typename message_reflection<M>::arguments>;
}
