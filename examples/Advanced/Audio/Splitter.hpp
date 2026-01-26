#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <algorithm>

namespace ao
{
class AudioSplitter
{
public:
  halp_meta(name, "Audio Splitter")
  halp_meta(c_name, "audio_splitter")
  halp_meta(category, "Audio/Utilities")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(
      description, "Split an incoming multi-channel input into individual channels")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#splitter")
  halp_meta(uuid, "42db08e5-625e-40d5-8f90-aed74cc34661")

  struct
  {
    halp::dynamic_audio_bus<"Input", double> audio;
    struct : halp::spinbox_i32<"Channels", halp::range{1, 1024, 2}>
    {
      static std::function<void(AudioSplitter&, int)> on_controller_interaction()
      {
        return [](AudioSplitter& object, int value) {
          object.outputs.audio.request_port_resize(std::clamp(value, 0, 4096));
        };
      }
    } channels;
  } inputs;

  struct
  {
    halp::dynamic_port<halp::audio_channel<"Channel {}", double>> audio;
  } outputs;

  using setup = halp::setup;
  void operator()(int frames)
  {
    const int requested_channels = inputs.channels;
    if(outputs.audio.ports.size() != requested_channels)
    {
      return;
    }

    const int count = std::min(inputs.audio.channels, requested_channels);
    for(int i = 0; i < count; i++)
    {
      double* ip = inputs.audio.samples[i];
      double* op = outputs.audio.ports[i].channel;

#pragma omp simd
      for(int frame = 0; frame < frames; ++frame)
        op[frame] += ip[frame];
    }
  }
};
}
