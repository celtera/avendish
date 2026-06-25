#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestImpulseButton
{
  halp_meta(name, "Test impulse button")
  halp_meta(c_name, "avnd_test_impulse")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "dd2ab385-1c44-4922-9e1f-abb46c4bf7df")
  struct { halp::impulse_button<"Bang"> bang; } inputs;
  struct { halp::val_port<"Count", int> count; } outputs;
  void operator()() { if(inputs.bang.value) outputs.count.value++; }
};
}
