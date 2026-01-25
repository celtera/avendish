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
struct output_introspection : fields_introspection<typename outputs_type<T>::type>
{
};

template <typename T>
auto get_outputs(avnd::effect_container<T>& t) -> decltype(t.outputs())
{
  return t.outputs();
}
template <avnd::outputs_is_type T>
auto& get_outputs(T& t)
{
#if !AVND_UNIQUE_INSTANCE
  AVND_ERROR(T, "Cannot get outputs on T");
#else
  static typename T::outputs r;
  return r;
#endif
}
template <avnd::outputs_is_value T>
auto& get_outputs(T& t)
{
  return t.outputs;
}

}
