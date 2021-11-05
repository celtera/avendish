#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <helpers/audio.hpp>
#include <helpers/controls.hpp>
#include <helpers/meta.hpp>
#include <common/concepts_polyfill.hpp>
#include <cmath>

namespace examples::helpers
{
struct PerSampleAsArgs
{
  $(name, "Per-sample processing (args, helpers)")
  $(c_name, "avnd_helpers_per_sample_as_args")
  $(uuid, "92a1a42b-dc63-42f8-8d24-8cb9c046803a")

  float operator()(float input)
  {
    return std::tanh(input);
  }
};

struct PerSampleAsPorts
{
  $(name, "Per-sample processing (ports, helpers)")
  $(c_name, "avnd_helpers_per_sample_as_ports")
  $(uuid, "31146933-c26b-4c21-a3f1-e991f70b520c")

  struct inputs
  {
    avnd::audio_sample<"In", double> audio;
  };

  struct outputs
  {
    avnd::audio_sample<"Out", double> audio;
  };

  void operator()(const inputs& ins, outputs& outs)
  {
    outs.audio.sample = std::tanh(ins.audio.sample);
  }
};
}
