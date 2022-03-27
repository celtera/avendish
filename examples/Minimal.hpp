#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
/**
 * This example shows that there is absolutely
 * no magic involved in the definition of the plug-ins:
 * it is technically possible to define nodes that would be viable
 * for an entirely bootstrapped environment with not a single standard header,
 * be it C or C++.
 * Of course this is only given as an example - things are a bit more verbose
 * than in other examples.
 */
struct Minimal
{
  // We need a minimal set of metadata.
  // name will be shown in environments which show a "pretty name", e.g. VSTs, etc.
  static consteval auto name() { return "Minimal example"; }

  // c_name is used for environments such as Pd, Max where object names can't have spaces
  // or weird characters
  static consteval auto c_name() { return "avnd_minimal"; }

  // Those are UUIDs in string form. Thankfully, it's possible to reduce those
  // to a binary form at compile time fairly easily (see score/uuid.hpp based on boost.uuid)
  static consteval auto uuid() { return "52eec529-484a-4509-bbc3-9d3455911327"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Grunge"; }
      // Controls are just pre-made types with some metadata
      // relevant for setting them up it up.
      static consteval auto range()
      {
        struct
        {
          const int min = 0;
          const int max = 30;
          const int init = 1;
        } c;
        return c;
      }

      int value;
    } gain;

    // This is an audio input port / bus.
    // The host will fill the samples buffers before calling operator()
    struct
    {
      // Same for the inlet / outlet metadata.
      static consteval auto name() { return "Input"; }
      double** samples;
      int channels;
    } audio;

  } inputs;

  // Likewise, samples can be assumed to be filled.
  struct
  {
    // This is an audio output port.
    struct
    {
      static consteval auto name() { return "Output"; }
      double** samples;
      int channels;
    } audio;
  } outputs;

  void operator()(int N)
  {
    const auto factor = inputs.gain.value;
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;

    // If nothing is specified, the host must ensure that
    // there will be as many channels available in output than in input.

    const auto chans = p1.channels;

    // Process the input buffer
    for (int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      // Cronch cronch cronch
      for (int j = 0; j < N; j++)
      {
        out[j] = in[j];
        for (int i = 0; i < factor; i++)
          out[j] *= in[j] * factor;
        out[j] *= in[j] * (1 + factor);
        while ((1. - out[j] * out[j]) > 1.0)
          out[j] = 1. - out[j];
        out[j] = out[j] < -0.999 ? -0.999 : out[j] > 0.999 ? 0.999 : out[j];
      }
    }
  }
};
}
