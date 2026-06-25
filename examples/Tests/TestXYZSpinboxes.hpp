#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestXYZSpinboxes
{
  halp_meta(name, "Test xyz spinboxes")
  halp_meta(c_name, "avnd_test_xyz")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "e981cbde-834a-4d73-876f-6afac7c5078f")
  struct { halp::xyz_spinboxes_f32<"In"> in; } inputs;
  struct { halp::xyz_spinboxes_f32<"Out"> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
