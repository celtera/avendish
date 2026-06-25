#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestLineEdit
{
  halp_meta(name, "Test line edit")
  halp_meta(c_name, "avnd_test_lineedit")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "c1b2916f-e5f0-4f64-a5d4-a3f43bc1bf9d")
  struct { halp::lineedit<"In", "hello"> in; } inputs;
  struct { halp::val_port<"Out", std::string> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
