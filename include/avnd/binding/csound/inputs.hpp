#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/csound/helpers.hpp> // <csdl.h> + macro scrub
#include <avnd/binding/csound/typestrings.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/effect_container.hpp>

namespace csound
{
/**
 * Reads the opcode's k-rate (and string) input arguments into the processor's
 * control parameters, in the canonical declaration order matching sig<T>.
 *
 * `args` is the full opcode args[] array; control inputs live at
 * sig<T>::ctrl_in_off + (predicate index of the parameter).
 */
template <typename T>
struct inputs
{
  static void read(avnd::effect_container<T>& impl, MYFLT** args)
  {
    if constexpr(sig<T>::n_ctrl_in > 0)
    {
      avnd::parameter_input_introspection<T>::for_all_n(
          avnd::get_inputs(impl),
          [&]<typename F, auto Idx>(F& field, avnd::predicate_index<Idx>) {
        MYFLT* slot = args[sig<T>::ctrl_in_off + Idx];
        if constexpr(avnd::string_parameter<F>)
        {
          // Csound stores a STRINGDAT* in the MYFLT* argument slot.
          auto* str = reinterpret_cast<STRINGDAT*>(slot);
          if(str && str->data)
            avnd::apply_control(field, str->data);
        }
        else
        {
          avnd::apply_control(field, (double)*slot);
        }
      });
    }
  }
};
}
