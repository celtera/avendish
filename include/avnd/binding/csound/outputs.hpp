#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/csound/helpers.hpp> // <csdl.h> + macro scrub
#include <avnd/binding/csound/typestrings.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/effect_container.hpp>

namespace csound
{
/**
 * Writes the processor's control / value output ports to the opcode's k-rate
 * output arguments, in the canonical order matching sig<T>.
 *
 * String outputs would require allocating a STRINGDAT through the engine; that
 * is deferred (rare for audio plug-ins), so only scalar outputs are emitted.
 */
template <typename T>
struct outputs
{
  static void write(avnd::effect_container<T>& impl, MYFLT** args)
  {
    if constexpr(sig<T>::n_ctrl_out > 0)
    {
      avnd::parameter_output_introspection<T>::for_all_n(
          avnd::get_outputs(impl),
          [&]<typename F, auto Idx>(F& field, avnd::predicate_index<Idx>) {
        if constexpr(!avnd::string_parameter<F>)
        {
          *args[sig<T>::ctrl_out_off + Idx] = (MYFLT)field.value;
        }
      });
    }
  }
};
}
