#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/enums.hpp>

#include <type_traits>

namespace avnd
{
/// Color reflection ///

enum class color_type
{
  darker,
  dark,
  mid,
  light,
  lighter,

  background_darker,
  background_dark,
  background_mid,
  background_light,
  background_lighter,

  runtime_value_light,
  runtime_value_mid,
  runtime_value_dark,

  editable_value_light,
  editable_value_mid,
  editable_value_dark,
};

template <typename T>
  requires std::is_enum_v<T>
constexpr auto get_color(T t)
{
  static_constexpr auto m = AVND_ENUM_MATCHER(
      (color_type::darker, darker, Darker), (color_type::dark, dark, Dark),
      (color_type::mid, mid, Mid), (color_type::light, light, Light),
      (color_type::lighter, lighter, Lighter),
      (color_type::background_darker, background_darker, BackgroundDarker),
      (color_type::background_dark, background_dark, BackgroundDark),
      (color_type::background_mid, background_mid, BackgroundMid),
      (color_type::background_light, background_light, BackgroundLight),
      (color_type::background_lighter, background_lighter, BackgroundLighter),
      (color_type::runtime_value_light, runtime_value_light, RuntimeValueLight),
      (color_type::runtime_value_mid, runtime_value_mid, RuntimeValueMid),
      (color_type::runtime_value_dark, runtime_value_dark, RuntimeValueDark),
      (color_type::editable_value_light, editable_value_light, EditableValueLight),
      (color_type::editable_value_mid, editable_value_light, EditableValueLight),
      (color_type::editable_value_dark, editable_value_light, EditableValueLight));
  return m(t, color_type::mid);
}

}
