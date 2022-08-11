#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <cmath>

namespace avnd
{
template <std::size_t N>
static constexpr int map_index_from_01(std::floating_point auto v)
{
  static_assert(N > 0);
  return std::round(v * (N - 1));
}

template <std::size_t N>
static constexpr double map_index_to_01(std::integral auto v)
{
  static_assert(N > 0);
  return (v + 0.5) / N;
}

template <avnd::parameter_with_values_range T>
static constexpr int closest_value_index(const auto& value)
{
  constexpr auto range = avnd::get_range<T>();
  static_assert(std::ssize(range.values) > 0);

  // TODO if we can check at compile-time that we are sorted,
  // then it's possible to do a lower_bound instead
  if constexpr(requires(T ctl) { ctl.value = range.values[0].second; })
  {
    int closest = 0;
    auto diff = std::abs(value - range.values[0].second);

    int k = 0;
    for(const auto& [str, newval] : range.values)
    {
      if(auto res = std::abs(value - newval); res < diff)
      {
        closest = k;
        diff = res;
      }
      k++;
    }
    return closest;
  }
  else if constexpr(requires(T ctl) { ctl.value = range.values[0]; })
  {
    int closest = 0;
    auto diff = std::abs(value - range.values[0]);

    int k = 0;
    for(const auto& newval : range.values)
    {
      if(auto res = std::abs(value - newval); res < diff)
      {
        closest = k;
        diff = res;
      }
      k++;
    }
    return closest;
  }
  else
  {
    return 0;
  }
}

template <avnd::parameter_with_values_range T>
static constexpr auto range_value(int index) -> std::decay_t<decltype(T::value)>
{
  constexpr auto range = avnd::get_range<T>();

  assert(index >= 0);
  assert(index < avnd::get_enum_choices_count<T>());

  if constexpr(requires(T ctl) { ctl.value = range.values[0].second; })
  {
    return range.values[index].second;
  }
  else if constexpr(requires(T ctl) { ctl.value = range.values[0]; })
  {
    return range.values[index];
  }
  else
  {
    static_assert(T::range_values_is_unsupported);
    return {};
  }
}
}
