#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestIntSlider
{
  halp_meta(name, "Test int slider")
  halp_meta(c_name, "avnd_test_int_slider")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "104e6f32-c346-4a5d-9a45-089d673abe2b")
  struct { halp::hslider_i32<"In", halp::irange{.min = -100, .max = 100, .init = 0}> in; } inputs;
  struct { halp::val_port<"Out", int> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
