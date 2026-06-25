#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.enums.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

#include <string>

namespace examples::tests
{
enum class Choice { Alpha, Beta, Gamma };
struct TestStringEnum
{
  halp_meta(name, "Test string enum")
  halp_meta(c_name, "avnd_test_string_enum")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "13b1cfa9-f687-46ff-bfea-d0d073c37d6e")
  struct { halp::string_enum_t<Choice, "In"> in; } inputs;
  struct { halp::val_port<"Out", std::string> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
