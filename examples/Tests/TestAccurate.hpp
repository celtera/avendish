#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/sample_accurate_controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestAccurate
{
  halp_meta(name, "Test sample-accurate control")
  halp_meta(c_name, "avnd_test_accurate")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "03575577-d13e-4c99-a821-5b069ed47e40")
  struct { halp::accurate<halp::hslider_f32<"In">> in; } inputs;
  struct { halp::val_port<"Out", float> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
