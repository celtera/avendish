#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>

#include <array>
#include <cstddef>

namespace csound
{
/**
 * Compile-time Csound opcode signature for an Avendish processor T.
 *
 * Csound writes one MYFLT* per declared argument into the opcode struct, in the
 * order "outputs first, then inputs". We lay everything out canonically and
 * derive BOTH the type-signature strings and the arg-slot offsets from the same
 * introspection walk so the two can never drift:
 *
 *   args = [ audio out (M × 'a') ][ ctrl out ('k'/'S') ]   ← outypes
 *          [ audio in  (N × 'a') ][ ctrl in  ('k'/'S') ]   ← intypes
 *
 * N/M (audio channel counts) come from a `halp::fixed_audio_bus<…,K>` when the
 * author fixes them; from a dynamic bus they default to mono (1); when the
 * processor has no audio port on a side, that side has 0 audio args.
 */
template <typename T>
struct sig
{
  // Presence is taken from bus_introspection (which covers both port-based audio
  // and the arg-based operator() shapes), the channel count from
  // input_channels<T>()/output_channels<T>() — a fixed_audio_bus<…,K> gives K,
  // a dynamic bus or an arg-based processor falls back to mono.
  static constexpr bool has_audio_in = avnd::bus_introspection<T>::input_busses > 0;
  static constexpr bool has_audio_out = avnd::bus_introspection<T>::output_busses > 0;

  static constexpr int n_audio_in = has_audio_in ? avnd::input_channels<T>(1) : 0;
  static constexpr int n_audio_out = has_audio_out ? avnd::output_channels<T>(1) : 0;

  static constexpr int n_ctrl_in = avnd::parameter_input_introspection<T>::size;
  static constexpr int n_ctrl_out = avnd::parameter_output_introspection<T>::size;

  static constexpr int n_in = n_audio_in + n_ctrl_in;
  static constexpr int n_out = n_audio_out + n_ctrl_out;

  // Offsets into the opcode struct's args[] array.
  static constexpr int audio_out_off = 0;
  static constexpr int ctrl_out_off = n_audio_out;
  static constexpr int audio_in_off = n_out; // inputs follow all outputs
  static constexpr int ctrl_in_off = n_out + n_audio_in;

  // i-time + perf-time (perf in the kopadr slot, aopadr unused — modern Csound
  // convention, cf. /usr/include/csound/plugin.h).
  static constexpr int thread = 3;

  static consteval auto build_intypes()
  {
    std::array<char, n_in + 1> r{};
    int k = 0;
    for(; k < n_audio_in; ++k)
      r[k] = 'a';
    avnd::parameter_input_introspection<T>::for_all([&]<typename F>(F) {
      r[k++] = avnd::string_parameter<typename F::type> ? 'S' : 'k';
    });
    r[n_in] = '\0';
    return r;
  }

  static consteval auto build_outypes()
  {
    std::array<char, n_out + 1> r{};
    int k = 0;
    for(; k < n_audio_out; ++k)
      r[k] = 'a';
    avnd::parameter_output_introspection<T>::for_all([&]<typename F>(F) {
      r[k++] = avnd::string_parameter<typename F::type> ? 'S' : 'k';
    });
    r[n_out] = '\0';
    return r;
  }

  static constexpr auto intypes = build_intypes();
  static constexpr auto outypes = build_outypes();
};
}
