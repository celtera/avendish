#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>

namespace examples
{
/**
 * Another example of per-sample processor.
 * This one uses ports instead of arguments.
 */
struct PerSampleProcessor2
{
  static consteval auto name() { return "Per-sample processor 2"; }
  static consteval auto c_name() { return "avnd_persample_2"; }
  static consteval auto uuid()
  {
    return "2de7ae80-4b07-410b-9e8b-eb9f49f9b6c5";
  }

  struct inputs
  {
    // Member named sample: it can be used as an audio input
    struct
    {
      static consteval auto name() { return "In"; }
      float sample;
    } audio;

    struct
    {
      static consteval auto name() { return "Gain"; }
      static consteval auto control()
      {
        struct
        {
          const float min = 0.001;
          const float max = 100;
          const float init = 1;
        } c;
        return c;
      }

      float value;
    } gain;
  };

  struct outputs
  {
    // Member named sample: it can be used as an audio output
    struct
    {
      static consteval auto name() { return "Out"; }
      float sample;
    } audio;
  };

  // Note that in this case, both inputs and outputs are passed as arguments
  void operator()(const inputs& ins, outputs& outs)
  {
    outs.audio.sample = std::tanh(ins.audio.sample * ins.gain.value);
  }
};
}
