#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <cmath>


namespace examples::helpers
{
// First example with classic array-based processing
struct PerBusAsArgs
{
  $(name, "Per-bus processing (args, helpers)")
  $(c_name, "avnd_helpers_per_bus_as_args")
  $(uuid, "23a4de57-800b-453a-99b6-481db19d834f")
  $(input_channels, 2)
  $(output_channels, 2)

  void operator()(double** inputs, double** outputs, int frames)
  {
    using namespace std;
    for(int c = 0; c < input_channels(); ++c) {
      for(int k = 0; k < frames; k++) {
        outputs[c][k] = tanh(inputs[c][k]);
      }
    }
  }
};

// Here we specify each bus's port count.
struct PerBusAsPortsFixed
{
  $(name, "Per-bus processing (fixed ports, helpers)")
  $(c_name, "avnd_helpers_per_bus_as_args")
  $(uuid, "0dd7b1df-7c84-49e9-8427-0532a87bccbe")

  struct
  {
    avnd::fixed_audio_bus<"In", double, 2> audio;
    avnd::fixed_audio_bus<"Sidechain", double, 2> sidechain;
  } inputs;

  struct
  {
    avnd::fixed_audio_bus<"Out", double, 2> audio;
  } outputs;

  void operator()(int frames)
  {
    using namespace std;

    for(int c = 0; c < inputs.audio.channels(); ++c) {
      for(int k = 0; k < frames; k++) {
        outputs.audio[c][k] = tanh(10*(inputs.audio[c][k] + inputs.sidechain[c][k]));
      }
    }
  }
};

// Special case: if there is only one dynamic_audio_bus (no channels specified)
// the host will choose the channels
// (or they will be taken from the global channels / input_channels / ... metadatas
struct PerBusAsPortsDynamic
{
  $(name, "Per-bus processing (dynamic ports, helpers)")
  $(c_name, "avnd_helpers_per_bus_as_args")
  $(uuid, "119d7020-6b7b-4dc9-af7d-ecfb23c5994d")

  struct
  {
    avnd::dynamic_audio_bus<"In", double> audio;
  } inputs;

  struct
  {
    avnd::dynamic_audio_bus<"Out", double> audio;
  } outputs;

  void operator()(int frames)
  {
    using namespace std;
    int max_channels = std::min(inputs.audio.channels, outputs.audio.channels);

    for(int c = 0; c < max_channels; ++c) {
      for(int k = 0; k < frames; k++) {
        outputs.audio[c][k] = tanh(inputs.audio[c][k]);
      }
    }
  }
};
}

