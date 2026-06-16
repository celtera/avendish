#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Classic (pre-reflection) backend for avnd::function_reflection<auto f>:
// one partial specialization per cv / ref / noexcept combination of member- and
// free-function pointers. Selected when C++26 static reflection is unavailable.
// The primary template is declared in function_reflection.hpp.

#include <boost/mp11/list.hpp>
#include <boost/predef.h>

#if defined(BOOST_COMP_GNUC)
#if BOOST_COMP_GNUC >= BOOST_VERSION_NUMBER(13, 0, 0)
#define AVND_NOEXCEPT_IN_TYPESYSTEM 1
#endif
#endif

namespace avnd
{
template <typename T, typename R, typename... Args, R (T::*member)(Args...)>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
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
  using class_type = T;
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
  using class_type = T;
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
  using class_type = T;
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
  using class_type = T;
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
  using class_type = T;
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
  using class_type = T;
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
  using class_type = T;
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
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

template <
    typename T, typename R, typename... Args, R (T::*member)(Args...) const volatile&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = true;
  static constexpr const bool is_rvalue_reference = false;
};

template <
    typename T, typename R, typename... Args, R (T::*member)(Args...) const volatile&&>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = false;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

// Noexcept versions
#if defined(AVND_NOEXCEPT_IN_TYPESYSTEM)
template <typename T, typename R, typename... Args, R (T::*member)(Args...) noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = false;
};

template <
    typename T, typename R, typename... Args, R (T::*member)(Args...) const noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = false;
};

template <
    typename T, typename R, typename... Args, R (T::*member)(Args...) volatile noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = false;
};

template <typename T, typename R, typename... Args, R (T::*member)(Args...)& noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = true;
  static constexpr const bool is_rvalue_reference = false;
};

template <typename T, typename R, typename... Args, R (T::*member)(Args...)&& noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

template <
    typename T, typename R, typename... Args, R (T::*member)(Args...) const& noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = true;
  static constexpr const bool is_rvalue_reference = false;
};

template <
    typename T, typename R, typename... Args, R (T::*member)(Args...) const&& noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = false;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

template <
    typename T, typename R, typename... Args, R (T::*member)(Args...) volatile& noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = true;
  static constexpr const bool is_rvalue_reference = false;
};

template <
    typename T, typename R, typename... Args,
    R (T::*member)(Args...) volatile&& noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = false;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

template <
    typename T, typename R, typename... Args,
    R (T::*member)(Args...) const volatile& noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = true;
  static constexpr const bool is_rvalue_reference = false;
};

template <
    typename T, typename R, typename... Args,
    R (T::*member)(Args...) const volatile&& noexcept>
struct function_reflection<member>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  using class_type = T;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_const = true;
  static constexpr const bool is_volatile = true;
  static constexpr const bool is_noexcept = true;
  static constexpr const bool is_reference = false;
  static constexpr const bool is_rvalue_reference = true;
};

template <typename R, typename... Args, R (*func)(Args...) noexcept>
struct function_reflection<func>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_noexcept = true;
};
#endif

template <typename R, typename... Args, R (*func)(Args...)>
struct function_reflection<func>
{
  using arguments = boost::mp11::mp_list<Args...>;
  using return_type = R;
  static constexpr const auto count = sizeof...(Args);
  static constexpr const bool is_noexcept = false;
};
}
