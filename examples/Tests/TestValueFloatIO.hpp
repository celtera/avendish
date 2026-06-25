#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestValueFloatIO
{
  halp_meta(name, "Test value float I/O")
  halp_meta(c_name, "avnd_test_val_float")
  halp_meta(category, "Tests/Values")
  halp_meta(uuid, "ac43bc65-e0aa-4094-bea9-a8e6ed67925b")
  struct { halp::val_port<"In", float> in; } inputs;
  struct { halp::val_port<"Out", float> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
