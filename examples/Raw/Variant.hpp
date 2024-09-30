#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <string>
#include <variant>

// clang-format on
namespace examples
{
struct Variant
{
  static consteval auto name() { return "Variant"; }
  static consteval auto c_name() { return "avnd_variant"; }
  static consteval auto uuid() { return "54926bf8-9cfc-47c4-8549-4b25a4a8200d"; }

  struct
  {
    struct
    {
      static constexpr auto name() { return "a"; }
      std::variant<float, std::string> value;
    } v;
    struct
    {
      static constexpr auto name() { return "b"; }
      std::variant<int, float, std::string> value;
    } v2;
  } inputs;

  struct
  {
    struct
    {
      static constexpr auto name() { return "a"; }
      std::variant<float, std::string> value;
    } v;
    struct
    {
      static constexpr auto name() { return "b"; }
      std::variant<int, float, std::string> value;
    } v2;
  } outputs;

  void operator()()
  {
    outputs.v.value = inputs.v.value;
    outputs.v2.value = inputs.v2.value;
  }
};
}
// clang-format on
