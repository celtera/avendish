#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/csound/helpers.hpp>
#include <avnd/binding/csound/inputs.hpp>
#include <avnd/binding/csound/outputs.hpp>
#include <avnd/binding/csound/typestrings.hpp>
#include <avnd/concepts/port.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <cstddef>
#include <new>

namespace csound
{
/**
 * Csound k-rate opcode wrapping an Avendish processor that has no audio ports
 * (a pure control / data object). It reads its control inputs, runs the
 * object's `operator()` once per control period when it has a no-argument form,
 * and writes its control outputs. Message dispatch is Phase 2.
 */
template <typename T>
struct message_opcode
{
  static constexpr int n_args
      = (sig<T>::n_out + sig<T>::n_in) > 0 ? (sig<T>::n_out + sig<T>::n_in) : 1;

  OPDS h;
  MYFLT* args[n_args];

  bool initialized;
  avnd::effect_container<T> implementation;

  static int deinit(CSOUND*, void* p) noexcept
  {
    auto* self = static_cast<message_opcode*>(p);
    if(self->initialized)
    {
      self->implementation.~effect_container<T>();
      self->initialized = false;
    }
    return OK;
  }

  int init(CSOUND* cs)
  {
    g_csound = cs;

    if(!initialized)
    {
      ::new(&implementation) avnd::effect_container<T>{};
      cs->RegisterDeinitCallback(cs, this, &message_opcode::deinit);
      initialized = true;
    }

    if constexpr(avnd::has_inputs<T>)
      avnd::init_controls(implementation);
    return OK;
  }

  int perf(CSOUND* cs)
  {
    g_csound = cs;

    if constexpr(avnd::has_inputs<T>)
      inputs<T>::read(implementation, args);

    for(auto state : implementation.full_state())
    {
      if constexpr(requires { state.effect(); })
        state.effect();
    }

    if constexpr(avnd::has_outputs<T>)
      outputs<T>::write(implementation, args);
    return OK;
  }

  static int register_opcode(CSOUND* cs)
  {
    static constexpr auto name = avnd::get_c_identifier<T>();
    return cs->AppendOpcode(
        cs, name.data(), (int)sizeof(message_opcode<T>), 0, sig<T>::thread,
        sig<T>::outypes.data(), sig<T>::intypes.data(),
        +[](CSOUND* c, void* p) { return static_cast<message_opcode*>(p)->init(c); },
        +[](CSOUND* c, void* p) { return static_cast<message_opcode*>(p)->perf(c); },
        nullptr);
  }
};
}
