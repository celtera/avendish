#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestBargraphOutput
{
  halp_meta(name, "Test bargraph output")
  halp_meta(c_name, "avnd_test_bargraph")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "f04286ac-2413-4b24-ae7e-d051a028ec11")
  struct { halp::hslider_f32<"In", halp::range{.min = 0., .max = 1., .init = 0.}> in; } inputs;
  struct { halp::hbargraph_f32<"Out", halp::range{.min = 0., .max = 1., .init = 0.}> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
