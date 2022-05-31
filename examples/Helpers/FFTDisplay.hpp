#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/ui.hpp>
#include <avnd/concepts/processor.hpp>
#include <avnd/concepts/painter.hpp>
#include <avnd/wrappers/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <cmath>
#include <variant>
#include <cstdio>

namespace examples::helpers
{
struct spectrum_display
{
  static constexpr double width() { return 500.; }
  static constexpr double height() { return 100.; }

  void paint(avnd::painter auto ctx)
  {
    ctx.set_fill_color({200, 200, 200, 255});
    ctx.begin_path();
    for(std::size_t c = 0; c < spectrums.size(); c++)
    {
      auto& spectrum = spectrums[c];
      double h = height() / spectrums.size();

      for(std::size_t i = 0; i < spectrum.size(); i++)
      {
        double barw = width() / spectrum.size();
        ctx.draw_rect(i * barw, c * h + (h - spectrum[i] * h), barw, spectrum[i] * h);
      }
    }
    ctx.fill();
  }

  std::vector<std::vector<float>> spectrums;
};

struct FFTDisplay
{
  static consteval auto name() { return "FFT Display example"; }
  static consteval auto c_name() { return "avnd_fft_displayu"; }
  static consteval auto uuid() { return "9eeadb52-209a-46ff-b4c6-d6c31d25aad6"; }

  struct {
    halp::dynamic_audio_spectrum_bus<"In", double> audio;
    static_assert(avnd::spectrum_split_bus_port<decltype(audio)>);
  } inputs;
  struct { } outputs;

  int k = 0;
  void operator()(int N)
  {
      if(k++ % 100 != 0)
          return;
    processor_to_ui p;
    const auto channels = inputs.audio.channels;
    p.spectrums.resize(channels);
    for(int i = 0; i < channels; i++) {
      auto& chan = p.spectrums[i];
      chan.resize(N/2);
      auto& ampl = inputs.audio.spectrum.amplitude[i];
      auto& ph = inputs.audio.spectrum.phase[i];
      for(int k = 0; k < N / 2; k++) {
        chan[k] = std::clamp(ampl[k] * ampl[k] + ph[k] * ph[k], 0., 1.);
      }
    }

    send_message(std::move(p));
  }

  struct processor_to_ui {
    std::vector<std::vector<float>> spectrums;
  };

  std::function<void(processor_to_ui)> send_message;

  struct ui {
    halp_meta(layout, halp::layouts::container)
    halp_meta(width, 200)
    halp_meta(height, 100)

    halp::custom_actions_item<spectrum_display> spectr;

    struct bus {
      static void process_message(ui& self, processor_to_ui msg)
      {
        self.spectr.spectrums = msg.spectrums;
      }
    };
  };
};
}
