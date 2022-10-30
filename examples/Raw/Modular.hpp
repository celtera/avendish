#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>

#include <numeric>

namespace examples
{
/**
 * An example fit for integration in modular, sample-based environments.
 */
struct Modular
{
  static consteval auto name() { return "Modular example"; }
  static consteval auto c_name() { return "avnd_modular"; }
  static consteval auto uuid() { return "2c82e37a-9caa-4e69-9255-2e131a2e6bab"; }

  struct
  {
    // Member named sample: it can be used as an audio input
    struct
    {
      static consteval auto name() { return "In L"; }
      float sample;
    } in_l;
    struct
    {
      static consteval auto name() { return "In R"; }
      float sample;
    } in_r;
    struct
    {
      static consteval auto name() { return "Sidechain L"; }
      float sample;
    } sidechain_l;
    struct
    {
      static consteval auto name() { return "Sidechain R"; }
      float sample;
    } sidechain_r;

    struct
    {
      static consteval auto name() { return "Gain"; }
      // Controls are just pre-made types with some metadata
      // relevant for setting them up it up.
      struct range
      {
        const float min = 0.001;
        const float max = 10.;
        const float init = 1;
      };

      float value;
    } gain;
  } inputs;

  struct
  {
    // Member named sample: it can be used as an audio output
    struct
    {
      static consteval auto name() { return "Out L"; }
      float sample;
    } out_l;
    struct
    {
      static consteval auto name() { return "Out R"; }
      float sample;
    } out_r;

    struct
    {
      static consteval auto name() { return "Level"; }
      float value;
    } level;
  } outputs;

  float clamp(float x, float min, float max)
  {
    return x < min ? min : x > max ? max : x;
  }

  void operator()()
  {
    using namespace std;
    outputs.out_l.sample = clamp(
        fmod(inputs.sidechain_l.sample * inputs.gain.value, abs(inputs.in_l.sample) + 0.01), -1.f,
        1.f);
    outputs.out_r.sample = clamp(
        fmod(inputs.sidechain_r.sample * inputs.gain.value, abs(inputs.in_r.sample) + 0.01), -1.f,
        1.f);

    outputs.level.value
        = sqrt(pow(outputs.out_l.sample, 2.) + pow(outputs.out_r.sample, 2.));
  }
};
}
