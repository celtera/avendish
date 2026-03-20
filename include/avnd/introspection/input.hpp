#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.hpp>
#include <avnd/common/index_sequence.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/effect_container.hpp>

namespace avnd
{

template <typename T>
struct input_introspection : fields_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
auto get_inputs(avnd::effect_container<T>& t) -> decltype(t.inputs())
{
  return t.inputs();
}
template <avnd::inputs_is_type T>
auto& get_inputs(T& t)
{
#if !AVND_UNIQUE_INSTANCE
  AVND_ERROR(T, "Cannot get inputs on T");
#else
  static typename T::inputs r;
  return r;
#endif
}
template <avnd::inputs_is_value T>
auto& get_inputs(T& t)
{
  return t.inputs;
}
}
