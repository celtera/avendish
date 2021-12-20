#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/avnd.hpp>

#include <cmath>

#include <string>

namespace avnd
{
/**
 * @brief Used for the case where the "host" works in a free frange but only with doubles
 */
template <avnd::float_parameter T>
static constexpr auto map_control_from_double(std::floating_point auto v)
{
  return v;
}

template <avnd::int_parameter T>
static constexpr auto map_control_from_double(std::floating_point auto v)
{
  return (int)v;
}

template <avnd::string_parameter T>
static constexpr auto map_control_from_double(std::floating_point auto v)
{
  return decltype(T::value){};
}

template <avnd::bool_parameter T>
static constexpr auto map_control_from_double(std::floating_point auto v)
{
  return v < 0.5 ? false : true;
}

template <avnd::enum_parameter T>
static constexpr auto map_control_from_double(std::floating_point auto v)
{
  constexpr auto min_val = 0;
  constexpr auto max_val = T::choices().size() - 1;
  int res = std::round(v);
  res = res < min_val ? min_val : res;
  res = res > max_val ? max_val : res;
  return static_cast<decltype(T::value)>(res);
}


template <typename T>
static constexpr auto map_control_from_double(std::floating_point auto v)
{
  static_assert(std::is_void_v<T>, "Error: unhandled control type");
}


template <avnd::float_parameter T>
static constexpr double map_control_to_double(const auto& value)
{
  return value;
}

template <avnd::int_parameter T>
static constexpr double map_control_to_double(const auto& value)
{
  return value;
}

template <avnd::string_parameter T>
static constexpr auto map_control_to_double(const auto& value)
{
  double v{};
  // TODO if we have a range, use it. Otherwise.. ?
  return v;
}

template <avnd::bool_parameter T>
static constexpr auto map_control_to_double(const auto& value)
{
  return value ? 1. : 0.;
}

template <avnd::enum_parameter T>
static constexpr auto map_control_to_double(const auto& value)
{
  static_assert(T::choices().size() > 0);
  return (double) static_cast<int>(value);
}

template <typename T>
static constexpr auto map_control_to_double(const auto& value)
{
  static_assert(std::is_void_v<T>, "Error: unhandled control type");
}

template <avnd::parameter T>
static constexpr auto map_control_to_double(const T& ctl)
{
  return map_control_to_double<T>(ctl.value);
}
}
