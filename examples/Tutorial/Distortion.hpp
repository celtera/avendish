#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <cmath>

#include <vector>

namespace examples
{
struct Distortion
{
  /**
   * An audio effect plug-in must provide some metadata: name, author, etc.
   * UUIDs are important to uniquely identify plug-ins: you can use uuidgen for instance.
   */
  avnd_meta(name, "My pretty distortion");
  avnd_meta(script_name, "disto_123");
  avnd_meta(category, "Demo");
  avnd_meta(author, "<AUTHOR>");
  avnd_meta(description, "<DESCRIPTION>");
  avnd_meta(uuid, "b9237d2a-1651-4dcc-920b-80e5e619c6c4");

  /** We define the input ports of our process: in this case,
   *  there's an audio input, a gain slider.
   */
  struct
  {
    halp::audio_input_bus<"Input"> audio;
    halp::hslider_f32<"Gain", halp::range{.min = 0.f, .max = 100.f, .init = 10.f}> gain;
  } inputs;

  /** And the output ports: only an audio output on this one.
   * We force stereo output no matter how many inputs there are.
   */
  struct
  {
    halp::fixed_audio_bus<"Output", double, 2> audio;
  } outputs;

  /** In this buffer, as an example we will store a mono signal. **/
  std::vector<float> temp_buffer;

  /** Will be called upon creation, and whenever the buffer size / sample rate changes **/
  void prepare(halp::setup info)
  {
    // Reserve some memory in our temporary mono buffer.
    temp_buffer.reserve(info.frames);

    // Note that channels in score are dynamic ; your process should not expect
    // a fixed number of channels, even across a single execution: it could be applied to 2 channels
    // at the start of a score, and 5 channels at the end.

    // It is still a work-in-progress to define a real-time-safe way to support this dynamic behaviour;
    // it is however guaranteed that there won't be other allocations during execution if
    // enough space had been reserved.
  }

  /**
   * Our actual function for transforming the inputs into outputs.
   *
   * Note that when using multichannel_audio_view and multichannel_audio,
   * there's no guarantee on the size of the data: on a token_request for 64 samples,
   * there may be for instance only 23 samples of 2-channel input,
   * while the output of the node could be 10 channels.
   *
   * It is up to the author to set-up the output according to its wishes for the plug-in.
   **/
  void operator()(int64_t N)
  {
    auto& gain = inputs.gain;

    // How many input channels
    const auto chans = inputs.audio.channels;

    // First sum all the input channels into a mono buffer
    temp_buffer.clear();
    temp_buffer.resize(N);
    for (int i = 0; i < chans; i++)
    {
      // The buffers are accessed through spans.
      auto in = inputs.audio.channel(i, N);

      // Sum samples from the input buffer
      const int64_t samples_to_read = N;
      for (int64_t j = 0; j < samples_to_read; j++)
      {
        temp_buffer[j] += in[j];
      }
    }

    // Then output a stereo buffer of that with distortion, and reversed phase.
    // We fix the output channels to 2 for this example.
    //const auto output_chans = 2;

    // We could have made things a bit simpler by just resizing our temp_buffer to N
    // and using that for the output ; this may save a few samples of silence though.
    const int64_t samples_to_write = std::min(N, int64_t(temp_buffer.size()));

    //output.resize(output_chans, samples_to_write);

    // Write to the channels once they are allocated:
    auto out_l = outputs.audio.channel(0, N);
    auto out_r = outputs.audio.channel(1, N);

    for (int64_t j = 0; j < samples_to_write; j++)
    {
      // Tanh
      out_l[j] = std::tanh(temp_buffer[j] * gain.value);

      // Bitcrush
      out_l[j] = int(out_l[j] * 4.) / 4.;

      // Invert phase on the right side
      out_r[j] = 1. - out_l[j];
    }
  }
};
}
