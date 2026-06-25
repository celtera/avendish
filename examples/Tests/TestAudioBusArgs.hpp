#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestAudioBusArgs
{
  halp_meta(name, "Test audio (bus as args)")
  halp_meta(c_name, "avnd_test_audio_bus_args")
  halp_meta(category, "Tests/Audio")
  halp_meta(input_channels, 2)
  halp_meta(output_channels, 2)
  halp_meta(uuid, "3375b4ea-4706-4710-9c6f-d440f75964a2")
  void operator()(double** ins, double** outs, int frames)
  {
    for(int c = 0; c < input_channels(); ++c)
      for(int k = 0; k < frames; k++)
        outs[c][k] = ins[c][k];
  }
};
}
