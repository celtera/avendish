#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
struct ExponentialSmoothing
{
  static constexpr auto name() { return "Exp Smoothing"; }
  static constexpr auto c_name() { return "avnd_expsmooth"; }
  static constexpr auto author() { return "Jean-Michaël Celerier"; }
  static constexpr auto category() { return "Control/Mappings"; }
  static constexpr auto description() { return "Simplest real-time value smoothing"; }
  static constexpr auto uuid() { return "971739ae-14a3-4283-b0a0-96dbd367ce66"; }

  // Allows the object to be seen as a value, not audio, processor
  enum
  {
    cv
  };

  struct
  {
    struct
    {
      static constexpr auto name() { return "Alpha"; }
      enum widget
      {
        hslider
      };
      struct range
      {
        double min{0.01}, max{1.}, init{0.5};
      };
      double value{range{}.init};
    } alpha;
  } inputs;

  struct
  {
  } outputs;

  double filtered{};
  double operator()(double in)
  {
    const double a = this->inputs.alpha.value;

    filtered = in * a + filtered * (1.0f - a);
    return filtered;
  }
};
}
