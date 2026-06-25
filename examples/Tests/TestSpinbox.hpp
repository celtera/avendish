#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestSpinbox
{
  halp_meta(name, "Test spinbox")
  halp_meta(c_name, "avnd_test_spinbox")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "a08df58a-de06-473b-a1b7-3a448f5cb34d")
  struct { halp::spinbox_i32<"In", halp::irange{.min = 0, .max = 1000, .init = 10}> in; } inputs;
  struct { halp::val_port<"Out", int> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
