#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/midi.hpp>
#include <avnd/helpers/controls.hpp>

#include <cmath>
#include <array>
#include <cstddef>
#include <cstdint>

namespace examples::helpers
{
/**
 * A very basic example of polyphonic MIDI synthesizer
 */
struct Midi
{
  $(name, "MIDI (helpers)")
  $(c_name, "avnd_helpers_midi")
  $(uuid, "4ac4f822-9e36-416e-8d15-e461268c7233")

  struct
  {
    avnd::midi_bus<"MIDI"> midi;
  } inputs;

  struct
  {
    avnd::midi_bus<"MIDI"> midi;
  } outputs;

  void operator()()
  {
    auto& i = inputs.midi;
    auto& o = outputs.midi;

    for(auto& message : i) {
      o.push_back(message);
    }
  }
};
}
