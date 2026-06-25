#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/soundfile_port.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestSoundfilePort
{
  halp_meta(name, "Test soundfile port")
  halp_meta(c_name, "avnd_test_soundfile")
  halp_meta(category, "Tests/IO")
  halp_meta(uuid, "b00496aa-2770-4ee0-a0be-171c1d2944b7")
  struct { halp::soundfile_port<"In"> in; } inputs;
  struct { halp::val_port<"Channels", int> channels; } outputs;
  void operator()() { outputs.channels.value = inputs.in.channels(); }
};
}
