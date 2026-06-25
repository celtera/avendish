#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestXYPad
{
  halp_meta(name, "Test xy pad")
  halp_meta(c_name, "avnd_test_xy")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "f426bb59-7a40-408d-b4ed-3818961f3d3c")
  struct { halp::xy_pad_f32<"In", halp::range{.min = -1., .max = 1., .init = 0.}> in; } inputs;
  struct { halp::val_port<"X", float> x; halp::val_port<"Y", float> y; } outputs;
  void operator()() { outputs.x.value = inputs.in.value.x; outputs.y.value = inputs.in.value.y; }
};
}
