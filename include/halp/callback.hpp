#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace halp
{

template <typename>
struct basic_callback;

template <typename R, typename... Args>
struct basic_callback<R(Args...)>
{
  using type = R(Args...);
  using func_t = R (*)(void*, Args...);
  func_t function;
  void* context;

  operator bool() const noexcept { return function; }

  template <typename... T>
  R operator()(T&&... args) const noexcept
  {
    return function(context, static_cast<T&&>(args)...);
  }
};

}
