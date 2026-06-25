#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestColor
{
  halp_meta(name, "Test color")
  halp_meta(c_name, "avnd_test_color")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "92f7a3a7-cd84-47d8-a5e3-f1e8f75587c6")
  struct { halp::color_chooser<"In"> in; } inputs;
  struct
  {
    halp::val_port<"R", float> r;
    halp::val_port<"G", float> g;
    halp::val_port<"B", float> b;
    halp::val_port<"A", float> a;
  } outputs;
  void operator()()
  {
    outputs.r.value = inputs.in.value.r;
    outputs.g.value = inputs.in.value.g;
    outputs.b.value = inputs.in.value.b;
    outputs.a.value = inputs.in.value.a;
  }
};
}
