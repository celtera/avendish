#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/static_string.hpp>

#include <cassert>
#include <cinttypes>
#include <string_view>

namespace halp
{

template <typename>
struct basic_callback;

template <typename R, typename... Args>
struct basic_callback<R(Args...)>
{
  using type = R(Args...);
  using func_t = R (*)(void*, Args...);
  func_t function{};
  void* context{};

  operator bool() const noexcept { return function; }

  template <typename... T>
  R operator()(T&&... args) const noexcept
  {
    assert(function);
    return function(context, static_cast<T&&>(args)...);
  }
};

template <static_string Name, typename... Args>
struct callback
{
  static consteval auto name() { return std::string_view{Name.value}; }
  basic_callback<void(Args...)> call;

  template <typename... T>
  void operator()(T&&... args) const noexcept
  {
    assert(call.function);
    return call.function(call.context, static_cast<T&&>(args)...);
  }
};

template <static_string Name, typename... Args>
struct timed_callback
{
  static consteval auto name() { return std::string_view{Name.value}; }
  enum
  {
    timestamp
  };
  basic_callback<void(int64_t, Args...)> call;

  template <typename... T>
  void operator()(T&&... args) const noexcept
  {
    assert(call.function);
    return call.function(call.context, static_cast<T&&>(args)...);
  }
};

}
