#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestFloatSlider
{
  halp_meta(name, "Test float slider")
  halp_meta(c_name, "avnd_test_float_slider")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "70047a30-6555-44f8-8b3b-f2d048006361")
  struct { halp::hslider_f32<"In", halp::range{.min = -10., .max = 10., .init = 1.}> in; } inputs;
  struct { halp::val_port<"Out", float> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
