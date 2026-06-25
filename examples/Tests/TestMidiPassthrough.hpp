#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/meta.hpp>
#include <halp/midi.hpp>

namespace examples::tests
{
struct TestMidiPassthrough
{
  halp_meta(name, "Test MIDI passthrough")
  halp_meta(c_name, "avnd_test_midi")
  halp_meta(category, "Tests/Midi")
  halp_meta(uuid, "b5f5d124-6b08-4cc3-adaf-f4ded9624309")
  struct { halp::midi_bus<"In"> midi; } inputs;
  struct { halp::midi_bus<"Out"> midi; } outputs;
  void operator()()
  {
    for(auto& m : inputs.midi)
      outputs.midi.push_back(m);
  }
};
}
