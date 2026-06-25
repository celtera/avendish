#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/midifile_port.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestMidifilePort
{
  halp_meta(name, "Test midifile port")
  halp_meta(c_name, "avnd_test_midifile")
  halp_meta(category, "Tests/IO")
  halp_meta(uuid, "1184a3be-35f2-48e3-9e29-03bfb5e8808b")
  struct { halp::midifile_port<"In"> in; } inputs;
  struct { halp::val_port<"Loaded", bool> loaded; } outputs;
  void operator()() { outputs.loaded.value = static_cast<bool>(inputs.in); }
};
}
