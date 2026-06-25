#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/midi.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestMidiOut
{
  halp_meta(name, "Test midi out")
  halp_meta(c_name, "avnd_test_midi_out")
  halp_meta(category, "Tests/MIDI")
  halp_meta(uuid, "04e64c1f-81b6-4896-a6f5-da8cdc361c05")
  struct { halp::val_port<"Note", int> note; } inputs;
  struct { halp::midi_out_bus<"Out"> midi; } outputs;
  void operator()()
  {
    outputs.midi.note_on(0, (uint8_t)inputs.note.value, 100);
  }
};
}
