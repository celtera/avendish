#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::helpers
{
// First example with classic array-based processing
struct PerBusAsArgs
{
  halp_meta(name, "Per-bus processing (args, helpers)")
  halp_meta(c_name, "avnd_helpers_per_bus_as_args1")
  halp_meta(uuid, "23a4de57-800b-453a-99b6-481db19d834f")
  halp_meta(input_channels, 2)
  halp_meta(output_channels, 2)

  void operator()(double** inputs, double** outputs, int frames)
  {
    using namespace std;
    for(int c = 0; c < input_channels(); ++c)
    {
      for(int k = 0; k < frames; k++)
      {
        outputs[c][k] = tanh(10. * inputs[c][k]);
      }
    }
  }
};

// Here we specify each bus's port count.
struct PerBusAsPortsFixed
{
  halp_meta(name, "Per-bus processing (fixed ports, helpers)")
  halp_meta(c_name, "avnd_helpers_per_bus_as_args")
  halp_meta(uuid, "0dd7b1df-7c84-49e9-8427-0532a87bccbe")

  struct
  {
    halp::fixed_audio_bus<"In", double, 2> audio;
    halp::fixed_audio_bus<"Sidechain", double, 2> sidechain;
  } inputs;

  struct
  {
    halp::fixed_audio_bus<"Out", double, 2> audio;
  } outputs;

  void operator()(int frames)
  {
    using namespace std;

    for(int c = 0; c < inputs.audio.channels(); ++c)
    {
      for(int k = 0; k < frames; k++)
      {
        outputs.audio[c][k] = tanh(10. * (inputs.audio[c][k] + inputs.sidechain[c][k]));
      }
    }
  }
};

// Special case: if there is only one dynamic_audio_bus (no channels specified)
// the host will choose the channels
// (or they will be taken from the global channels / input_channels / ... metadatas
struct PerBusAsPortsDynamic
{
  halp_meta(name, "Per-bus processing (dynamic ports, helpers)")
  halp_meta(c_name, "avnd_helpers_per_bus_as_args")
  halp_meta(uuid, "119d7020-6b7b-4dc9-af7d-ecfb23c5994d")

  struct
  {
    halp::dynamic_audio_bus<"In", double> audio;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Out", double> audio;
  } outputs;

  void operator()(int frames)
  {
    using namespace std;
    int max_channels = std::min(inputs.audio.channels, outputs.audio.channels);

    for(int c = 0; c < max_channels; ++c)
    {
      for(int k = 0; k < frames; k++)
      {
        outputs.audio[c][k] = tanh(10. * inputs.audio[c][k]);
      }
    }
  }
};
}
