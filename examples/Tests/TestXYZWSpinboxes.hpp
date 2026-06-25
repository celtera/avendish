#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestXYZWSpinboxes
{
  halp_meta(name, "Test xyzw spinboxes")
  halp_meta(c_name, "avnd_test_xyzw")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "9654b899-d672-41c8-ae83-ec83451aa2a6")
  struct { halp::xyzw_spinboxes_f32<"In"> in; } inputs;
  struct { halp::xyzw_spinboxes_f32<"Out"> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
