#pragma once

#include <vector>

/* SPDX-License-Identifier: GPL-3.0-or-later */

// clang-format off
namespace examples
{
struct DoubleArray
{
  static consteval auto name() { return "Double Array"; }
  static consteval auto c_name() { return "avnd_doublearray"; }
  static consteval auto category() { return "Control/Math"; }
  static consteval auto description() { return "Multiply every value in an input array by a scalar."; }
  static consteval auto uuid() { return "fe91368b-8e01-4603-b111-931dbd51d27f"; }

  struct
  {
    struct { static consteval auto name() { return "In"; } std::vector<float> value; } a;
    struct {
      static consteval auto name() { return "Gain"; }
      enum widget { knob };
      struct range {
        float min = 0.f;
        float max = 10.f;
        float init = 2.0f;
      };
      float value = 2.0f;
    } b;
  } inputs;

  struct
  {
    struct { static consteval auto name() { return "Out"; } std::vector<float> value; } out;
  } outputs;

  void operator()() {
    const auto N = inputs.a.value.size();
    outputs.out.value.resize(N);
    for(int i = 0; i < N; i++)
      outputs.out.value[i] = inputs.a.value[i] * inputs.b.value;
  }
};
}
// clang-format on
