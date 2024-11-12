#pragma once
#include <halp/audio.hpp>
#include <halp/controls.hpp>

#include <algorithm>

namespace examples::helpers
{
struct AudioChannelSelector
{
  static constexpr auto name() { return "Audio Channel Extractor"; }
  static constexpr auto c_name() { return "avnd_channel_extractor"; }
  static constexpr auto author() { return "Jean-Michaël Celerier"; }
  static consteval auto category() { return "Audio/Utilities"; }
  static consteval auto manual_url()
  {
    return "https://ossia.io/score-docs/processes/"
           "audio-utilities.html#audio-channel-extractor";
  }
  static constexpr auto uuid() { return "5a0b7b2c-4f1e-40e7-8d27-10bdcb856ddf"; }

  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    struct : halp::spinbox_i32<"First channel", halp::range{0, 128, 0}>
    {
      void update(AudioChannelSelector& self) { self.reconfigure(); }
    } first_channel;
    struct : halp::spinbox_i32<"Last channel", halp::range{0, 128, 4}>
    {
      void update(AudioChannelSelector& self) { self.reconfigure(); }
    } last_channel;
  } inputs;
  struct
  {
    halp::variable_audio_bus<"Audio", double> audio;
  } outputs;

  void prepare(halp::setup s) { }

  void reconfigure()
  {
    if(inputs.last_channel > inputs.first_channel)
      outputs.audio.request_channels(inputs.last_channel - inputs.first_channel);
  }

  void operator()(int frames)
  {
    for(int c_in = inputs.first_channel, c_out = 0; c_in < inputs.last_channel;
        c_in++, c_out++)
    {
      if(c_in < inputs.audio.channels)
      {
        std::span<double> in = inputs.audio.channel(c_in, frames);
        std::span<double> out = outputs.audio.channel(c_out, frames);

        std::copy_n(in.data(), frames, out.data());
      }
    }
  }
};
}
