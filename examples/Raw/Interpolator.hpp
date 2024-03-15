#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <chrono>
#include <utility>
#include <vector>

namespace examples
{
struct ExponentialSmoothing
{
  static constexpr auto name() { return "Exponential Smoothing"; }
  static constexpr auto c_name() { return "avnd_expsmooth"; }
  static constexpr auto uuid() { return "971739ae-14a3-4283-b0a0-96dbd367ce66"; }

  struct
  {
    struct
    {
      static constexpr auto name() { return "Input"; }
      double value{};
    } in;

    struct
    {
      static constexpr auto name() { return "Alpha"; }
      struct range
      {
        double min{0.01}, max{1.}, init{0.5};
      };
      double value{range{}.init};
    } alpha;
  } inputs;

  struct
  {
    struct
    {
      static constexpr auto name() { return "Output"; }
      double value{};
    } out;
  } outputs;

  double filtered{};
  void operator()()
  {
    const double in = this->inputs.in.value;
    const double a = this->inputs.alpha.value;

    filtered = in * a + filtered * (1.0f - a);
    outputs.out.value = filtered;
  }
};
}
