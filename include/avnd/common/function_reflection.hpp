#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/meta_polyfill.hpp>

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>

#include <functional>
#include <type_traits>

namespace avnd
{

template <typename T>
concept function = std::is_function_v<std::remove_pointer_t<std::remove_reference_t<T>>>;

template <class T>
struct looks_like_std_function : std::false_type
{
};

template <template <typename...> typename F, typename R, typename... Args>
struct looks_like_std_function<F<R(Args...)>> : std::true_type
{
};

template <
    template <typename...> typename F, typename R, typename... Args, typename... Rem>
struct looks_like_std_function<F<R(Args...), Rem...>> : std::true_type
{
};

template <typename T>
concept function_ish = looks_like_std_function<std::decay_t<T>>::value;

// Reflects a free- or member-function pointer: ::arguments (mp_list), ::return_type,
// ::class_type (members only), ::count and the cv/ref/noexcept flags.
template <auto f>
struct function_reflection;
}

// Backend providing the function_reflection<auto f> specializations / definition.
#if AVND_USE_STD_REFLECTION
#include <avnd/common/function_reflection.p2996.hpp>
#else
#include <avnd/common/function_reflection.classic.hpp>
#endif

namespace avnd
{
template <auto F>
using first_argument = boost::mp11::mp_first<typename function_reflection<F>::arguments>;
template <auto F>
using second_argument
    = boost::mp11::mp_second<typename function_reflection<F>::arguments>;
template <auto F>
using third_argument = boost::mp11::mp_third<typename function_reflection<F>::arguments>;

// For std::function-like things
template <typename T>
struct function_reflection_t;
template <typename R, typename... Args>
struct function_reflection_t<R(Args...)>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = false;
};

template <typename R, typename... Args>
struct function_reflection_t<R (&)(Args...)>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = false;

  static constexpr auto synthesize()
  {
    if constexpr(std::is_void_v<R>)
      return [](Args...) -> void { return; };
    else
      return [](Args...) -> R { return {}; };
  }
};

template <template <typename...> typename F, typename R, typename... Args>
struct function_reflection_t<F<R(Args...)>> : function_reflection_t<R(Args...)>
{
};

template <typename T>
using first_argument_t
    = boost::mp11::mp_first<typename function_reflection_t<T>::arguments>;
template <typename T>
using second_argument_t
    = boost::mp11::mp_second<typename function_reflection_t<T>::arguments>;
template <typename T>
using third_argument_t
    = boost::mp11::mp_third<typename function_reflection_t<T>::arguments>;

// For functors / lambdas
template <typename T>
struct function_reflection_o;
template <typename T>
  requires requires { function_reflection<&T::operator()>{}; }
struct function_reflection_o<T> : function_reflection<&T::operator()>
{
};

}
