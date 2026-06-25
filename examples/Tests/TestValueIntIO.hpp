#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestValueIntIO
{
  halp_meta(name, "Test value int I/O")
  halp_meta(c_name, "avnd_test_val_int")
  halp_meta(category, "Tests/Values")
  halp_meta(uuid, "1677688b-840d-463c-a5af-f23099ca41c1")
  struct { halp::val_port<"In", int> in; } inputs;
  struct { halp::val_port<"Out", int> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
