#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestToggle
{
  halp_meta(name, "Test toggle")
  halp_meta(c_name, "avnd_test_toggle")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "b24e5d88-fe81-4a01-aab3-f4dc9dfae552")
  struct { halp::toggle<"In", halp::toggle_setup{.init = true}> in; } inputs;
  struct { halp::val_port<"Out", bool> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
