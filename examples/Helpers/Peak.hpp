#pragma once
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <cmath>

namespace examples::helpers
{

struct Peak
{
  $(name, "Peak value")
  $(c_name, "avnd_peak")
  $(uuid, "57d3476d-9dbb-45ac-b76e-a2a51b48b8af")

  struct
  {
    avnd::audio_channel<"In", double> audio;
  } inputs;

  struct
  {
    avnd::val_port<"Peak", double> peak;
  } outputs;

  void operator()(int frames)
  {
    outputs.peak = 0.;
    for(int k = 0; k < frames; k++) {
      double sample = std::abs(inputs.audio[k]);
      if(sample > outputs.peak)
        outputs.peak = sample;
    }
  }
};

}

