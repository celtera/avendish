#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestFloatKnob
{
  halp_meta(name, "Test float knob")
  halp_meta(c_name, "avnd_test_float_knob")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "8aeee7a8-19af-4f19-a20f-95fc2540125d")
  struct { halp::knob_f32<"In", halp::range{.min = 0., .max = 1., .init = 0.5}> in; } inputs;
  struct { halp::val_port<"Out", float> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
