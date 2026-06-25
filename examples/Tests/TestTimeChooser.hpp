#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestTimeChooser
{
  halp_meta(name, "Test time chooser")
  halp_meta(c_name, "avnd_test_time")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "81145d08-097f-431c-8afe-8fd32f1a9e05")
  struct { halp::time_chooser<"In"> in; } inputs;
  struct { halp::time_chooser<"Out"> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
