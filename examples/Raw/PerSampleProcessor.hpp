#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>

#include <numeric>

namespace examples
{
/**
 * An example of monophonic, per-sample processor.
 */
struct PerSampleProcessor
{
  static consteval auto name() { return "Per-sample processor"; }
  static consteval auto c_name() { return "avnd_persample_1"; }
  static consteval auto uuid() { return "c0ece845-2df7-486d-b096-6d507cbe23d1"; }

  // For per-sample processors, it is better to have inputs as a type:
  // a single space for the inputs will be allocated for the polyphonic voices.
  struct inputs
  {
    struct
    {
      static consteval auto name() { return "Gain"; }
      // Controls are just pre-made types with some metadata
      // relevant for setting them up it up.
      static consteval auto range()
      {
        struct
        {
          const float min = 0.001;
          const float max = 500.0;
          const float init = 1;
        } c;
        return c;
      }

      float value;
    } gain;
  };

  struct outputs
  {
  };

  // Per-tick information can be passed that way.
  // Concepts would allow to check that a given "struct tick"
  // would indeed get the expected informatoin.
  struct tick
  {
    float tempo{120.};
  };

  // Internal state will be replicated across instances if necessary, e.g.
  // if the host wants to make a polyphonic effect out of this one.
  float internal_state{};

  // Process one sample.
  float operator()(float input, const inputs& ins, outputs& outs, tick tick)
  {
    float x = std::fmod(input * ins.gain.value, internal_state + 1.0f);
    float out = x < -0.5f ? -0.5f : x > 0.5f ? 0.5f : x;
    internal_state = std::exp(out) + 0.1;
    return out;
  }
};

}
