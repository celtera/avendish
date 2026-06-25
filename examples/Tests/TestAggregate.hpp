#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <string>

namespace examples::tests
{
struct TestAggregate
{
  halp_meta(name, "Test aggregate")
  halp_meta(c_name, "avnd_test_aggregate")
  halp_meta(category, "Tests/Structured")
  halp_meta(uuid, "320856dd-d62f-4fc7-ad60-958634f17e65")

  struct Value
  {
    float x;
    int y;
    std::string z;
    halp_field_names(x, y, z);
  };

  struct
  {
    struct { halp_meta(name, "In") Value value; } in;
  } inputs;
  struct
  {
    struct { halp_meta(name, "Out") Value value; } out;
  } outputs;

  void operator()() { outputs.out.value = inputs.in.value; }
};
}
