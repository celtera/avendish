#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <ossia/audio/audio_device.hpp>
#include <ossia/audio/audio_engine.hpp>

namespace standalone
{

struct audio_tick
{
  int frames;
};

template <typename T>
struct audio_mapper
{
  avnd::effect_container<T>& object;
  ossia::audio_engine* audio{};

  [[no_unique_address]] avnd::process_adapter<T> processor;

  explicit audio_mapper(
      avnd::effect_container<T>& object, int in_channels, int out_channels, int bs,
      int rate)
      : object{object}
  {
    int want_ins = avnd::input_channels<T>(in_channels);
    int want_outs = avnd::output_channels<T>(out_channels);

    int want_buffer_size = bs;
    int want_sample_rate = rate;
    audio = ossia::make_audio_engine(
        "", std::string(avnd::get_name<T>()), "", "", want_ins, want_outs,
        want_sample_rate, want_buffer_size);

    // Allocate buffers that may be required for converting float <-> double
    avnd::process_setup setup_info{
        .input_channels = want_ins,
        .output_channels = want_outs,
        .frames_per_buffer = want_buffer_size,
        .rate = (double)want_sample_rate};

    processor.allocate_buffers(setup_info, float{});
    processor.allocate_buffers(setup_info, double{});

    avnd::prepare(object, setup_info);
    audio->set_tick([this](auto& st) { (*this)(st); });
  }

  void operator()(const ossia::audio_tick_state& st)
  {
    audio_tick tick = {.frames = (int)st.frames};
    int n = avnd::get_frames(tick);
    auto ins = avnd::span<float*>{const_cast<float**>(st.inputs), (std::size_t)st.n_in};
    auto outs = avnd::span<float*>{st.outputs, (std::size_t)st.n_out};
    this->processor.process(object, ins, outs, tick);
  }

  ~audio_mapper()
  {
    audio->stop();
    delete audio;
  }
};
}
