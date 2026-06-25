#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestBypass
{
  halp_meta(name, "Test bypass")
  halp_meta(c_name, "avnd_test_bypass")
  halp_meta(category, "Tests/Lifecycle")
  halp_meta(uuid, "f01735ea-3fe9-4963-8a3a-39e0f98df03f")
  bool bypass = false;
  struct { halp::val_port<"In", float> in; } inputs;
  struct { halp::val_port<"Out", float> out; } outputs;
  void operator()() { outputs.out.value = bypass ? 0.f : inputs.in.value; }
};
}
