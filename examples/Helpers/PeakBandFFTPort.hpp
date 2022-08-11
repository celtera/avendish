#pragma once
#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/fft.hpp>
#include <halp/meta.hpp>

namespace examples::helpers
{

/**
 * For the usual case where we just want the spectrum of an input buffer,
 * no need to template: we can ask it to be precomputed beforehand by the host.
 */
struct PeakBandFFTPort
{
  halp_meta(name, "Peak band (FFT port)")
  halp_meta(c_name, "avnd_peak_band_fft_port")
  halp_meta(uuid, "143f5cb8-d0b1-44de-a1a4-ccd5315192fa")

  // TODO implement user-controllable buffering to allow various fft sizes...
  int buffer_size = 1024;

  struct
  {
    // Here the host will fill audio.spectrum with a windowed FFT.
    // Option A (an helper type is provided)
    halp::audio_spectrum_channel<"In", double> audio;

    // Option B with the raw spectrum ; no window is defined.
    struct
    {
      halp_meta(name, "In 2");

      double* channel{};
      // complex numbers... using value_type = double[2] is also ok
      struct
      {
        std::complex<double>* bin;
      } spectrum{};
    } audio_2;
  } inputs;

  struct
  {
    halp::val_port<"Peak", double> peak;
    halp::val_port<"Band", int> band;

    halp::val_port<"Peak 2", double> peak_2;
    halp::val_port<"Band 2", int> band_2;
  } outputs;

  void operator()(int frames)
  {
    // Process with option A
    {
      outputs.peak = 0.;

      // Compute the band with the highest amplitude
      for(int k = 0; k < frames / 2; k++)
      {
        const double ampl = inputs.audio.spectrum.amplitude[k];
        const double phas = inputs.audio.spectrum.phase[k];
        const double mag_squared = ampl * ampl + phas * phas;

        if(mag_squared > outputs.peak)
        {
          outputs.peak = mag_squared;
          outputs.band = k;
        }
      }

      outputs.peak = std::sqrt(outputs.peak);
    }

    // Process with option B
    {
      outputs.peak_2 = 0.;

      // Compute the band with the highest amplitude
      for(int k = 0; k < frames / 2; k++)
      {
        const double mag_squared = std::norm(inputs.audio_2.spectrum.bin[k]);

        if(mag_squared > outputs.peak_2)
        {
          outputs.peak_2 = mag_squared;
          outputs.band_2 = k;
        }
      }

      outputs.peak_2 = std::sqrt(outputs.peak_2);
    }
  }
};

}
