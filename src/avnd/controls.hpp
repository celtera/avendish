#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/avnd.hpp>

#include <string>

namespace avnd
{
static void init_controls(auto& inputs)
{
  boost::pfr::for_each_field(
      inputs,
      []<typename C>(C& ctl)
      {
        if constexpr (requires { C::control().init; })
          ctl.value = C::control().init;
      });
}

/**
 * @brief Used for the case where the "host" can handle any
 * range of value: the plug-in will report the range,
 * and then we just have to clip to be safe
 */
template <typename T>
static void apply_control(T& ctl, float v)
{
  // Apply the value
  ctl.value = v;

  // Clamp
  if constexpr (requires { T::control().min; })
  {
    constexpr auto c = T::control();
    if (ctl.value < c.min)
      ctl.value = c.min;
    else if (ctl.value > c.max)
      ctl.value = c.max;
  }
}

static void apply_control(auto& ctl, std::string&& v)
{
  // Apply the value
  ctl.value = std::move(v);

  // Clamp in range
  TODO;
}

/**
 * @brief Used for the case where the "host" works in a fixed [0. ; 1.] range
 */
template <typename T>
static void map_control_from_01(T& ctl, float v)
{
  // Apply the value
  if constexpr (requires { ctl.value = v; })
  {
    if constexpr (requires { T::control().min; })
    {
      constexpr auto c = T::control();
      ctl.value = c.min + v * (c.max - c.min);
    }
    else
    {
      ctl.value = v;
    }
  }
  else if constexpr (requires { ctl.value = ""; })
  {
    // TODO if we have a range, use it
  }
  else
  {
    static_assert(std::is_void_v<T>, "Error: unhandled control type");
  }
}

template <typename T>
static float map_control_to_01(const T& ctl)
{
  // Apply the value
  float v{};
  if constexpr (requires { v = ctl.value; })
  {
    if constexpr (requires { T::control().min; })
    {
      constexpr auto c = T::control();

      v = (ctl.value - c.min) / (c.max - c.min);
    }
    else
    {
      v = ctl.value;
    }
  }
  // FIXME better concept for string-ish parameters
  else if constexpr (requires
                     {
                       {
                         ctl.value
                         } -> std::convertible_to<const char*>;
                     })
  {
    // TODO if we have a range, use it
  }
  else
  {
    static_assert(std::is_void_v<T>, "Error: unhandled control type");
  }
  return v;
}
}
