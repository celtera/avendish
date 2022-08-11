#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace examples
{
/**
 * This example showcases using user-defined aggregates for value ports.
 */
struct Aggregate
{
  static consteval auto name() { return "Aggregate Input"; }
  static consteval auto c_name() { return "avnd_aggregate"; }
  static consteval auto uuid() { return "a66648f4-85d9-46dc-9a11-0b8dc700e1af"; }

  struct TestAggregate
  {
    int a, b;
    float c;
    struct
    {
      std::vector<float> a;
      std::string b;
      std::array<bool, 4> c;
    } sub;
  };

  struct
  {
    struct
    {
      static consteval auto name() { return "In"; }
      TestAggregate value;
    } agg;
  } inputs;

  struct
  {
    struct
    {
      static consteval auto name() { return "Out"; }
      TestAggregate value;
    } agg;
  } outputs;

  void operator()() { outputs.agg.value = inputs.agg.value; }
};
}
