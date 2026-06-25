#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>
#include <string>
namespace examples::tests
{
struct TestValueStringIO
{
  halp_meta(name, "Test value string I/O")
  halp_meta(c_name, "avnd_test_val_string")
  halp_meta(category, "Tests/Values")
  halp_meta(uuid, "f2dc29b8-ca59-4e6d-8b82-9947a5c216f6")
  struct { halp::val_port<"In", std::string> in; } inputs;
  struct { halp::val_port<"Out", std::string> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
