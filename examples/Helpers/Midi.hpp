#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <cmath>

#include <array>
#include <cstddef>
#include <cstdint>

namespace examples::helpers
{
/**
 * A very basic example of MIDI pass-through
 */
struct Midi
{
  $(name, "MIDI (helpers)")
  $(c_name, "avnd_helpers_midi")
  $(uuid, "4ac4f822-9e36-416e-8d15-e461268c7233")

  struct
  {
    halp::midi_bus<"MIDI"> midi;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI"> midi;
  } outputs;

  void operator()()
  {
    auto& i = inputs.midi;
    auto& o = outputs.midi;

    for (auto& message : i)
    {
      o.push_back(message);
    }
  }
};
}
