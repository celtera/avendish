#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <vector>
#include <cmath>
#include <ratio>

namespace examples
{

/**
 * Basic example of sine generator, to showcase widget mapping functions
 */
struct Sine
{
  static constexpr auto name() { return "Sines"; }
  static constexpr auto c_name() { return "avnd_sines"; }
  static constexpr auto uuid() { return "ce5469ca-1be1-4846-b690-4f4d0081ecab"; }

  // FIXME: when clang supports double arguments we can use them here instead
  template<typename Ratio>
  struct log_mapper
  {
    static constexpr double mid = double(Ratio::num)/double(Ratio::den);
    static constexpr double b = (1./mid - 1. ) * (1./mid - 1.);
    static constexpr double a = 1. / (b - 1.);
    static double map(double v) noexcept { return a * (std::pow(b, v) - 1.); }
    static double unmap(double v) noexcept { return std::log(v / a + 1.) / std::log(b); }
  };

  struct frequency_range
  {
    const float min = 20.;
    const float max = 2000.;
    const float init = 200.;
  };

  struct inputs
  {
    struct
    {
      static constexpr auto name() { return "Log m=0.85"; }
      using mapper = log_mapper<std::ratio<85, 100>>;
      using range = frequency_range;
      float value;
    } f1;

    struct
    {
      static constexpr auto name() { return "Log m=0.15"; }
      using mapper = log_mapper<std::ratio<15, 100>>;
      using range = frequency_range;
      float value;
    } f2;

    struct
    {
      static constexpr auto name() { return "Cubic"; }
      struct mapper
      {
        static double map(double v) noexcept { return v * v * v; }
        static double unmap(double v) noexcept { return std::pow(v, 1. / 3.); }
      };

      using range = frequency_range;
      float value;
    } f3;

    struct
    {
      static constexpr auto name() { return "Linear (default)"; }
      using range = frequency_range;
      float value;
    } f4;
  };

  struct outputs
  {
    struct {
      static consteval auto name() { return "Out"; }
      float sample;
    } audio;
  };

  struct setup
  {
    float rate{};
  } setup;

  void prepare(struct setup info)
  {
    this->setup = info;
  }

  double phase[4] = { 0. };
  void operator()(const inputs& inputs, outputs& outputs)
  {
    const double phi[4] = {
      2 * 3.14 * inputs.f1.value / setup.rate,
      2 * 3.14 * inputs.f2.value / setup.rate,
      2 * 3.14 * inputs.f3.value / setup.rate,
      2 * 3.14 * inputs.f4.value / setup.rate,
    };
    outputs.audio.sample = 0.;
    for(int i = 0; i < 4; i++) {
      outputs.audio.sample += 0.15 * sin(phase[i] += phi[i]);
    }
  }
};
}
