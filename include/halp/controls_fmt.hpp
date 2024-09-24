#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#if __has_include(<fmt/format.h>) && !defined(AVND_DISABLE_FMT)
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace fmt
{
template <typename T>
struct formatter<halp::combo_pair<T>>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) const noexcept
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const halp::combo_pair<T>& number, FormatContext& ctx) const noexcept
  {
    return fmt::format_to(ctx.out(), "combo: {}->{}", number.first, number.second);
  }
};

template <typename T>
struct formatter<halp::xy_type<T>>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) const noexcept
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const halp::xy_type<T>& number, FormatContext& ctx) const noexcept
  {
    return fmt::format_to(ctx.out(), "xy: {}, {}", number.x, number.y);
  }
};

template <>
struct formatter<halp::color_type>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) const noexcept
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const halp::color_type& number, FormatContext& ctx) const noexcept
  {
    return fmt::format_to(
        ctx.out(), "rgba: {}, {}, {}, {}", number.r, number.g, number.b, number.a);
  }
};

template <>
struct formatter<halp::impulse_type>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) const noexcept
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const halp::impulse_type& number, FormatContext& ctx) const noexcept
  {
    return fmt::format_to(ctx.out(), "impulse");
  }
};

template <typename T>
struct formatter<halp::range_slider_value<T>>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) const noexcept
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const halp::range_slider_value<T>& number, FormatContext& ctx) const noexcept
  {
    return fmt::format_to(ctx.out(), "range: {} -> {}", number.start, number.end);
  }
};

}
#endif
