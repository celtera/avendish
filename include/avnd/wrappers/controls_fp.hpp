#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/ranges.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <cmath>
#include <halp/polyfill.hpp>

#include <string>

namespace avnd
{

/// Some APIs like VST3 only support fp parameters.
/// Thus we have a conversion step from normalized 0-1 FP to full-range FP, and then from that to the actual type.

template <avnd::float_parameter T>
static constexpr auto map_control_from_01_to_fp(std::floating_point auto v)
{
  if constexpr(avnd::parameter_with_minmax_range<T>)
  {
    static_constexpr auto c = avnd::get_range<T>();
    return c.min + v * (c.max - c.min);
  }
  else if constexpr(avnd::parameter_with_values_range<T>)
  {
    // Here we just map from / to the index of the value

    // [0;1] -> [0;N[
    const auto mapped = map_index_from_01<avnd::get_enum_choices_count<T>()>(v);

    // [0;N[ -> actual value
    return range_value<T>(mapped);
  }
  else
  {
    return v;
  }
}

template <avnd::int_parameter T>
static constexpr auto map_control_from_01_to_fp(std::floating_point auto v)
{
  if constexpr(avnd::parameter_with_minmax_range<T>)
  {
    static_constexpr auto c = avnd::get_range<T>();
    return c.min + v * (c.max - c.min);
  }
  else if constexpr(avnd::parameter_with_values_range<T>)
  {
    // Here we just map from / to the index of the value

    // [0;1] -> [0;N[
    const auto mapped = map_index_from_01<avnd::get_enum_choices_count<T>()>(v);

    // [0;N[ -> actual value
    return range_value<T>(mapped);
  }
  else
  {
    return v;
  }
}

template <avnd::bool_parameter T>
static constexpr auto map_control_from_01_to_fp(std::floating_point auto v)
{
  return v < 0.5 ? 0. : 1.;
}

template <avnd::string_parameter T>
static constexpr auto map_control_from_01_to_fp(std::floating_point auto v)
{
  // TODO ??
  return v;
}

// e.g. here:
// - the input domain is [0; 1]
// - the output domain is [0; number of enumerations - 1]
template <avnd::enum_parameter T>
static constexpr auto map_control_from_01_to_fp(std::floating_point auto v)
{
  return double(std::round(v * (avnd::get_enum_choices_count<T>() - 1)));
}

template <typename T>
static constexpr auto map_control_from_01_to_fp(std::floating_point auto v) = delete;
// {
//   static_assert(std::is_void_v<T>, "Error: unhandled control type");
// }

template <avnd::float_parameter T>
static constexpr auto map_control_from_fp_to_01(std::floating_point auto value)
{
  // Apply the value
  double v{};
  if constexpr(avnd::parameter_with_minmax_range<T>)
  {
    static_constexpr auto c = avnd::get_range<T>();

    v = (value - c.min) / double(c.max - c.min);
  }
  else if constexpr(avnd::parameter_with_values_range<T>)
  {
    const int idx = closest_value_index<T>(value);
    v = map_index_to_01<avnd::get_enum_choices_count<T>()>(idx);
  }
  else
  {
    v = value;
  }
  return v;
}

template <avnd::int_parameter T>
static constexpr auto map_control_from_fp_to_01(std::floating_point auto value)
{
  // Apply the value
  double v{};
  if constexpr(avnd::parameter_with_minmax_range<T>)
  {
    // TODO generalize
    static_assert(avnd::get_range<T>().max != avnd::get_range<T>().min);
    static_constexpr auto c = avnd::get_range<T>();

    v = (value - c.min) / double(c.max - c.min);
  }
  else if constexpr(avnd::parameter_with_values_range<T>)
  {
    const int idx = closest_value_index<T>(value);
    v = map_index_to_01<avnd::get_enum_choices_count<T>()>(idx);
  }
  else
  {
    v = value;
  }
  return v;
}

template <avnd::string_parameter T>
static constexpr auto map_control_from_fp_to_01(std::floating_point auto value)
{
  double v{};
  // TODO if we have a range, use it. Otherwise.. ?
  return v;
}

template <avnd::bool_parameter T>
static constexpr auto map_control_from_fp_to_01(std::floating_point auto value)
{
  return value;
}

template <avnd::enum_parameter T>
static constexpr auto map_control_from_fp_to_01(std::floating_point auto value)
{
  static_assert(avnd::get_enum_choices_count<T>() > 0);
  return ((value + 0.5) / avnd::get_enum_choices_count<T>());
}

template <typename T>
static constexpr auto map_control_from_fp_to_01(std::floating_point auto value) = delete;
// {
//   static_assert(std::is_void_v<T>, "Error: unhandled control type");
// }

}
