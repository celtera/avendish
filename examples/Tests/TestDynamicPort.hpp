#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/dynamic_port.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestDynamicPort
{
  halp_meta(name, "Test dynamic port")
  halp_meta(c_name, "avnd_test_dynamic_port")
  halp_meta(category, "Tests/IO")
  halp_meta(uuid, "118a741a-09e2-44a8-bb6f-4d2d48b8c87d")
  struct { halp::dynamic_port<halp::val_port<"In {}", float>> in; } inputs;
  struct { halp::val_port<"Sum", float> sum; } outputs;
  void operator()()
  {
    float s = 0.f;
    for(auto& p : inputs.in.ports) s += p.value;
    outputs.sum.value = s;
  }
};
}
