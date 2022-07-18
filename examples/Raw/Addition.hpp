#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
struct Addition
{
  static consteval auto name() { return "Addition"; }
  static consteval auto c_name() { return "avnd_addition"; }
  static consteval auto uuid() { return "36427eb1-b5f4-4735-a383-6164cb9b2572"; }

  struct
  {
    struct { float value; } a;
    struct { float value; } b;
  } inputs;

  struct
  {
    struct { float value; } out;
  } outputs;

  void operator()() { outputs.out.value = inputs.a.value + inputs.b.value; }
};
}
