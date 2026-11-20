#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/csound/helpers.hpp>
#include <avnd/binding/csound/inputs.hpp>
#include <avnd/binding/csound/outputs.hpp>
#include <avnd/binding/csound/typestrings.hpp>
#include <avnd/common/span_polyfill.hpp>
#include <avnd/concepts/port.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/prepare.hpp>
#include <avnd/wrappers/process_adapter.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>

namespace csound
{
/**
 * Csound opcode wrapping an Avendish audio processor.
 *
 * Csound allocates `sizeof(audio_opcode)` zero-initialised bytes and fills the
 * `args[]` pointers before calling init() (i-time) then perf() (each k-cycle).
 * The non-trivial C++ members are placement-new'd over that memory at i-time and
 * destroyed through a registered deinit callback (note deactivation / reset).
 */
template <typename T>
struct audio_opcode
{
  static constexpr int n_args
      = (sig<T>::n_out + sig<T>::n_in) > 0 ? (sig<T>::n_out + sig<T>::n_in) : 1;
  static constexpr int in_chans = sig<T>::n_audio_in;
  static constexpr int out_chans = sig<T>::n_audio_out;

  // ── Csound-managed: opcode header + one pointer per declared argument ──
  OPDS h;
  MYFLT* args[n_args];

  // ── Our private state, constructed at i-time over the engine's memory ──
  bool initialized;
  avnd::effect_container<T> implementation;
  avnd::process_adapter<T> processor;

  static int deinit(CSOUND*, void* p) noexcept
  {
    auto* self = static_cast<audio_opcode*>(p);
    if(self->initialized)
    {
      self->processor.~process_adapter<T>();
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
      ::new(&processor) avnd::process_adapter<T>{};
      cs->RegisterDeinitCallback(cs, this, &audio_opcode::deinit);
      initialized = true;
    }

    const avnd::process_setup setup{
        .input_channels = in_chans,
        .output_channels = out_chans,
        .frames_per_buffer = (int)h.insdshead->ksmps,
        .rate = (double)cs->GetSr(cs)};

    processor.allocate_buffers(setup, MYFLT{});
    avnd::prepare(implementation, setup);

    if constexpr(avnd::has_inputs<T>)
      avnd::init_controls(implementation);

    implementation.init_channels(in_chans, out_chans);
    return OK;
  }

  int perf(CSOUND* cs)
  {
    g_csound = cs;

    const uint32_t ksmps = h.insdshead->ksmps;
    const uint32_t offset = h.insdshead->ksmps_offset;
    const uint32_t early = h.insdshead->ksmps_no_end;
    const uint32_t nsmps = ksmps - early;

    // Pull k-rate / string control inputs into the parameters.
    if constexpr(avnd::has_inputs<T>)
      inputs<T>::read(implementation, args);

    // Csound passes one MYFLT[ksmps] vector per audio channel; advance every
    // pointer by `offset` so we only touch the sample-accurate active region.
    std::array<MYFLT*, in_chans> in;
    for(int c = 0; c < in_chans; ++c)
      in[c] = args[sig<T>::audio_in_off + c] + offset;

    std::array<MYFLT*, out_chans> out;
    for(int c = 0; c < out_chans; ++c)
    {
      MYFLT* o = args[sig<T>::audio_out_off + c];
      // Sample-accuracy: silence the lead-in and tail samples we won't write.
      if(offset > 0)
        std::fill_n(o, offset, MYFLT{});
      if(early > 0)
        std::fill_n(o + nsmps, early, MYFLT{});
      out[c] = o + offset;
    }

    processor.process(
        implementation, avnd::span<MYFLT*>{in.data(), (std::size_t)in_chans},
        avnd::span<MYFLT*>{out.data(), (std::size_t)out_chans},
        (int)(nsmps - offset));

    // Push control / value outputs.
    if constexpr(avnd::has_outputs<T>)
      outputs<T>::write(implementation, args);

    return OK;
  }

  static int register_opcode(CSOUND* cs)
  {
    static constexpr auto name = avnd::get_c_identifier<T>();
    return cs->AppendOpcode(
        cs, name.data(), (int)sizeof(audio_opcode<T>), 0, sig<T>::thread,
        sig<T>::outypes.data(), sig<T>::intypes.data(),
        +[](CSOUND* c, void* p) { return static_cast<audio_opcode*>(p)->init(c); },
        +[](CSOUND* c, void* p) { return static_cast<audio_opcode*>(p)->perf(c); },
        nullptr);
  }
};
}
