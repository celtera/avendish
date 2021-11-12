#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/concepts/generic.hpp>
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/function_reflection.hpp>

#include <string_view>

namespace avnd
{

template <typename T>
concept stateless_message =
    requires{ typename avnd::function_reflection<T::func()>::return_type; }
;
template <typename T>
concept stateful_message =
    requires{ typename avnd::function_reflection_o<decltype(T::func())>::return_type; }
;

template <typename T>
concept reflectable_message =
    stateless_message<T> || stateful_message<T>
;

template <typename T>
concept message = reflectable_message<T> && requires(T t)
{
  {
    t.name()
    } -> string_ish;
};

type_or_value_qualification(messages) type_or_value_reflection(messages)


template<typename M>
consteval auto message_function_reflection()
{
  if constexpr (requires { avnd::function_reflection<M::func()>::count; })
  {
    return avnd::function_reflection<M::func()>{};
  }
  else if constexpr (requires { avnd::function_reflection_o<decltype(M::func())>::count; })
  {
    return avnd::function_reflection_o<decltype(M::func())>{};
  }
  else
  {
    return;
  }
}

template<typename M>
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
