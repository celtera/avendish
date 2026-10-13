#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
/**
 * A processor whose parameters come AFTER an audio port, so their raw field
 * indices differ from their dense ordinals: exercises the id space of the
 * parameter and state paths in the bindings.
 */
struct MixedParams
{
  static constexpr auto name() { return "MixedParams"; }
  static constexpr auto c_name() { return "avnd_mixed_params"; }
  static constexpr auto uuid() { return "16d3f7c2-8b7e-4a52-b64f-2c1a90aa7e55"; }

  struct
  {
    struct
    {
      static constexpr auto name() { return "In"; }
      const double** samples{};
      int channels{};
    } audio;

    struct
    {
      static constexpr auto name() { return "Mix"; }
      struct range
      {
        double min = 0., max = 1., init = 1.;
      };
      double value = 1.;
    } mix;

    struct
    {
      static constexpr auto name() { return "Freq"; }
      struct range
      {
        double min = 20., max = 20000., init = 440.;
      };
      double value = 440.;
    } freq;

    struct
    {
      static constexpr auto name() { return "Mode"; }
      struct range
      {
        int min = 0, max = 3, init = 0;
      };
      int value = 0;
    } mode;
  } inputs;

  struct
  {
    struct
    {
      static constexpr auto name() { return "Out"; }
      double** samples{};
      int channels{};
    } audio;
  } outputs;

  void operator()(int frames)
  {
    for(int c = 0; c < outputs.audio.channels; c++)
      for(int i = 0; i < frames; i++)
        outputs.audio.samples[c][i]
            = (c < inputs.audio.channels ? inputs.audio.samples[c][i] : 0.)
              * inputs.mix.value;
  }
};
}
