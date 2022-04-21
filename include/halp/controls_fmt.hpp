#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <fmt/format.h>

namespace fmt
{
template <typename T>
struct formatter<halp::combo_pair<T>>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const halp::combo_pair<T>& number, FormatContext& ctx)
  {
    return fmt::format_to(ctx.out(), "combo: {}->{}", number.first, number.second);
  }
};

template <typename T>
struct formatter<halp::xy_type<T>>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const halp::xy_type<T>& number, FormatContext& ctx)
  {
    return fmt::format_to(ctx.out(), "xy: {}, {}", number.x, number.y);
  }
};
template <>
struct formatter<halp::color_type>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const halp::color_type& number, FormatContext& ctx)
  {
    return fmt::format_to(
        ctx.out(), "rgba: {}, {}, {}, {}", number.r, number.g, number.b, number.a);
  }
};
}
