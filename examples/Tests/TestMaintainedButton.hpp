#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestMaintainedButton
{
  halp_meta(name, "Test maintained button")
  halp_meta(c_name, "avnd_test_maintained")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "abfb8663-c7e0-43b3-918b-9f11090d1754")
  struct { halp::maintained_button<"In"> in; } inputs;
  struct { halp::val_port<"Out", bool> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
