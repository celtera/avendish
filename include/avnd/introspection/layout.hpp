#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/layout.hpp>
#include <avnd/wrappers/effect_container.hpp>

namespace avnd
{
template <typename T>
struct ui_introspection : fields_introspection<typename ui_type<T>::type>
{
};

template <typename T>
auto& get_ui(avnd::effect_container<T>& t)
{
  using type = typename avnd::ui_type<T>::type;
  static const constexpr type ui;
  return ui;
}

template <avnd::ui_is_type T>
auto& get_ui(T& t)
{
  using type = typename avnd::ui_type<T>::type;
  static const constexpr type ui;
  return ui;
}

template <avnd::ui_is_value T>
auto& get_ui(T& t)
{
  return t.ui;
}

}
