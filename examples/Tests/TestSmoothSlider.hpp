#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/smooth_controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestSmoothSlider
{
  halp_meta(name, "Test smooth slider")
  halp_meta(c_name, "avnd_test_smooth")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "ec050855-3e23-434b-b5c7-2b9f481edc14")
  struct { halp::smooth_slider<"In"> in; } inputs;
  struct { halp::val_port<"Out", float> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
