#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

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
    template <typename...>
    typename F,
    typename R,
    typename... Args,
    typename... Rem>
struct looks_like_std_function<F<R(Args...), Rem...>> : std::true_type
{
};

template <typename T>
concept function_ish = looks_like_std_function<std::decay_t<T>>::value;

template <auto f>
struct function_reflection;

template <typename T, typename R, typename... Args, R (T::*member)(Args...)>
struct function_reflection<member>
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

template <typename T, typename R, typename... Args, R (T::*member)(Args...) const>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = false;
};

template <typename T, typename R, typename... Args, R (T::*member)(Args...) volatile>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = false;
};

template <typename T, typename R, typename... Args, R (T::*member)(Args...)&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = true;
  static constexpr const bool is_rvalue_reference = false;
};

template <typename T, typename R, typename... Args, R (T::*member)(Args...) &&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

template <typename T, typename R, typename... Args, R (T::*member)(Args...) const&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = true;
  static constexpr const bool is_rvalue_reference = false;
};

template <typename T, typename R, typename... Args, R (T::*member)(Args...) const&&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

template <typename T, typename R, typename... Args, R (T::*member)(Args...) volatile&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = true;
  static constexpr const bool is_rvalue_reference = false;
};

template <typename T, typename R, typename... Args, R (T::*member)(Args...) volatile&&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

template <
    typename T,
    typename R,
    typename... Args,
    R (T::*member)(Args...) const volatile&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = true;
  static constexpr const bool is_rvalue_reference = false;
};

template <
    typename T,
    typename R,
    typename... Args,
    R (T::*member)(Args...) const volatile&&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

template <typename R, typename... Args, R (*func)(Args...)>
struct function_reflection<func>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_noexcept = false;
};

#if !defined(_MSC_VER)
template <typename R, typename... Args, R (*func)(Args...) noexcept>
struct function_reflection<func>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_noexcept = true;
};
#endif

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
    if constexpr (std::is_void_v<R>)
      return [](Args...) -> void { return; };
    else
      return [](Args...) -> R { return {}; };
  }
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
