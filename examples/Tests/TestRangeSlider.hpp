#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestRangeSlider
{
  halp_meta(name, "Test range slider")
  halp_meta(c_name, "avnd_test_range_slider")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "7f20458a-18b0-44d9-9139-16e49a1a33fb")
  struct { halp::range_slider_f32<"In"> in; } inputs;
  struct { halp::range_slider_f32<"Out"> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
