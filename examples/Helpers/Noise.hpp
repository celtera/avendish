#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/meta.hpp>
#include <cmath>

namespace examples::helpers
{
struct WhiteNoise
{
  halp_meta(name, "White noise")
  halp_meta(c_name, "avnd_white_noise")
  halp_meta(uuid, "5e98f775-b242-4cca-9a3e-1e74662a2c7d")

  struct inputs
  {
  };

  struct outputs
  {
    halp::audio_sample<"Out", double> audio;
  };

  void operator()(const inputs& ins, outputs& outs)
  {
    outs.audio.sample = 0.15 * (double(rand()) / RAND_MAX - 0.5);
  }
};
}
