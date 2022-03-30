#pragma once
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/fft.hpp>
#include <cmath>

namespace examples::helpers
{

/**
 * This examples shows how one can get an FFT implementation to be
 * dependency-injected at compile-time. Check the Logger example for more explanations.
 *
 * The idea is that the host software / plugin wrapper will provide its own FFT implementation.
 * This way, we don't end up with 450 different FFT implementations in a single binary,
 * and the algorithm is abstracted from the actual way to compute the FFT which does not
 * matter - it could be FFTW, MKL, etc... depending on the licensing requirements
 * of the project, or be left up to the host which will instantiate the plug-in.
 */

template <halp::has_fft_1d<double> C>
struct PeakBand
{
  avnd_meta(name, "Peak band")
  avnd_meta(c_name, "avnd_peak_band")
  avnd_meta(uuid, "5610b62e-ef1f-4a34-abe0-e57816bc44c2")

  struct
  {
    halp::audio_channel<"In", double> audio;
  } inputs;

  struct
  {
    halp::val_port<"Peak", double> peak;
    halp::val_port<"Band", int> band;
  } outputs;

  // Instantiate the FFT provided by the configuration.
  // Syntax is a bit ugly as we are already in a template
  // causing the need for the "::template " thing ; in C++23
  // it should be possible to omit typename.
  using fft_type = typename C::template fft_type<double>;
  fft_type fft;

  void prepare(halp::setup info)
  {
    // Initialize potential internal FFT buffers
    fft.reset(info.frames);
  }

  void operator()(int frames)
  {
    outputs.peak = 0.;

    // Process the fft
    auto cplx = fft.execute(inputs.audio.channel, frames);

    // Compute the band with the highest amplitude
    for (int k = 0; k < frames / 2; k++)
    {
      const double mag_squared = std::norm(cplx[k]);
      if (mag_squared > outputs.peak)
      {
        outputs.peak = mag_squared;
        outputs.band = k;
      }
    }

    outputs.peak = std::sqrt(outputs.peak);
  }
};

}
