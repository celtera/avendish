#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/modules.hpp>

HALP_MODULE_EXPORT
namespace halp
{

template <typename T>
struct reactive_value
{
  T value;

  basic_callback<void(const T&)> notify;

  operator T&() noexcept { return value; }
  operator const T&() const noexcept { return value; }

  reactive_value& operator=(const T& t)
  {
    value = t;
    if(notify)
      notify(value);
  }

  reactive_value& operator=(T&& t)
  {
    value = t;
    if(notify)
      notify(value);
  }
};

}
