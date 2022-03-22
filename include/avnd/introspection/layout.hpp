#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/layout.hpp>
#include <avnd/wrappers/effect_container.hpp>

namespace avnd
{
template <typename T>
struct layout_introspection : fields_introspection<typename layout_type<T>::type> { };

template <typename T>
auto& get_layout(avnd::effect_container<T>& t)
{
  using type = typename avnd::layout_type<T>::type;
  static const constexpr type layout;
  return layout;
}

template <avnd::layout_is_type T>
auto& get_layout(T& t)
{
  using type = typename avnd::layout_type<T>::type;
  static const constexpr type layout;
  return layout;
}

template <avnd::layout_is_value T>
auto& get_layout(T& t)
{
  return t.layout;
}

}
