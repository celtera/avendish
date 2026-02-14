#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/processor.hpp>
#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::helpers
{
struct PerFrameAsPorts
{
  halp_meta(name, "Per-frame processing (ports, helpers)")
  halp_meta(c_name, "avnd_helpers_per_frame_as_ports")
  halp_meta(uuid, "cdb48431-be3c-41e2-8c2d-7e68fa69a9e3")

  struct inputs
  {
    halp::dynamic_audio_frame<"In", double> audio;
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 10., .init = 5}> gain;
  };

  struct outputs
  {
    halp::dynamic_audio_frame<"Out", double> audio;
  };

  void operator()(const inputs& ins, outputs& outs)
  {
    for(int i = 0; i < ins.audio.channels && i < outs.audio.channels; i++)
      outs.audio[i] = std::tanh(ins.gain * ins.audio[i]);
  }
};

static_assert(avnd::polyphonic_processor<double, PerFrameAsPorts>);
static_assert(avnd::per_frame_port_processor<double, PerFrameAsPorts>);
static_assert(avnd::frame_port_processor<PerFrameAsPorts>);
static_assert(avnd::inputs_is_type<PerFrameAsPorts>);
static_assert(avnd::outputs_is_type<PerFrameAsPorts>);
}
