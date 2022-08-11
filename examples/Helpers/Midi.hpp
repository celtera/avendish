#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

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
  halp_meta(name, "MIDI (helpers)")
  halp_meta(c_name, "avnd_helpers_midi")
  halp_meta(uuid, "4ac4f822-9e36-416e-8d15-e461268c7233")

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

    for(auto& message : i)
    {
      o.push_back(message);
    }
  }
};
}
