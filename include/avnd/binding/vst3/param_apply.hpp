#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/input.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_double.hpp>
#include <avnd/wrappers/effect_container.hpp>

namespace stv3
{
/**
 * Apply one host parameter change (raw field index, normalized [0, 1] value)
 * to every effect instance, invoking the port's update() callback with the
 * new value in place, like init_controls does.
 *
 * Kept free of SDK types so it is testable on its own.
 */
template <typename T>
inline void
apply_control(avnd::effect_container<T>& effect, int id, double value) noexcept
{
  // Apply the host parameter change to every (per-channel) instance so all
  // voices of a polyphonic effect track automation, not just instance 0.
  for(auto state : effect.full_state())
    avnd::parameter_input_introspection<T>::for_nth_raw(
        state.inputs, id, [&]<typename C>(C& ctl) {
          if constexpr(requires { avnd::map_control_from_01<C>(value); })
            assign_if_assignable(ctl.value, avnd::map_control_from_01<C>(value));
          if_possible(ctl.update(state.effect));
        });
}
}
