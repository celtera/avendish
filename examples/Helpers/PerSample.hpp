#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/processor.hpp>
#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::helpers
{
struct PerSampleAsArgs
{
  halp_meta(name, "Per-sample processing (args, helpers)")
  halp_meta(c_name, "avnd_helpers_per_sample_as_args")
  halp_meta(uuid, "92a1a42b-dc63-42f8-8d24-8cb9c046803a")

  float operator()(float input) { return std::tanh(10. * input); }
};

struct PerSampleAsArgs2
{
  halp_meta(name, "Per-sample processing (args, helpers)")
  halp_meta(c_name, "avnd_helpers_per_sample_as_args2")
  halp_meta(uuid, "cfedd0f1-4c75-4b59-98ff-f38f83a6bb80")

  struct inputs
  {
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 10., .init = 5}> gain;
  };

  struct outputs
  {
  };

  float operator()(float input, const inputs& ins, const outputs& out)
  {
    return std::tanh(ins.gain * input);
  }
};

struct PerSampleAsPorts
{
  halp_meta(name, "Per-sample processing (ports, helpers)")
  halp_meta(c_name, "avnd_helpers_per_sample_as_ports")
  halp_meta(uuid, "31146933-c26b-4c21-a3f1-e991f70b520c")

  struct inputs
  {
    halp::audio_sample<"In", double> audio;
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 10., .init = 5}> gain;
  };

  struct outputs
  {
    halp::audio_sample<"Out", double> audio;
  };

  void operator()(const inputs& ins, outputs& outs)
  {
    outs.audio = std::tanh(ins.gain * ins.audio);
  }
};

static_assert(avnd::monophonic_processor<double, PerSampleAsPorts>);
static_assert(avnd::mono_per_sample_port_processor<double, PerSampleAsPorts>);
static_assert(avnd::sample_port_processor<PerSampleAsPorts>);
static_assert(avnd::inputs_is_type<PerSampleAsPorts>);
static_assert(avnd::outputs_is_type<PerSampleAsPorts>);
}
