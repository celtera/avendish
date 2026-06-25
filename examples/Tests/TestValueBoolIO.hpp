#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestValueBoolIO
{
  halp_meta(name, "Test value bool I/O")
  halp_meta(c_name, "avnd_test_val_bool")
  halp_meta(category, "Tests/Values")
  halp_meta(uuid, "703200e1-1682-40ed-912c-e4b0c709e5fc")
  struct { halp::val_port<"In", bool> in; } inputs;
  struct { halp::val_port<"Out", bool> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
