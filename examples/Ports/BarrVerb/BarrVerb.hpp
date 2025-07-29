#pragma once

/*
  BarrVerb reverb plugin - Avendish port
  Ported from: github.com/ErroneousBosh/BarrVerb/

  Original License:
  ISC License
  Copyright 2024 Gordon JC Pearce <gordonjcp@gjcp.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
  OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "rom.h"

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <vector>

namespace ao
{
struct BarrVerb
{
  halp_meta(name, "BarrVerb")
  halp_meta(c_name, "barrverb")
  halp_meta(category, "Audio/Effects/Reverb")
  halp_meta(description, "Alesis MIDIVerb emulation, a tribute to Keith Barr")
  halp_meta(author, "Gordonjcp")
  halp_meta(uuid, "eb97ea63-3def-4e71-b577-444df5637ea8")

  // Use ROM data from rom.h
  static constexpr const uint16_t* rom = barrverb_rom;
  static constexpr const char* const* prog_name = barrverb_prog_names;

  // Inputs
  struct
  {
    halp::fixed_audio_bus<"In", double, 2> audio;

    halp::spinbox_i32<"Program", halp::range{1, 64, 20}> program;
    halp::knob_f32<"Mix", halp::range{0., 1., 0.25}> mix;
  } inputs;

  // Outputs
  struct
  {
    halp::fixed_audio_bus<"Out", double, 2> audio;
  } outputs;

  // Internal state
  class SVF
  {
  public:
    constexpr void setFreq(float cutoff, float q, float samplerate)
    {
      z1 = z2 = 0;

      w = 2.f * std::tan(3.14159f * (cutoff / samplerate));
      a = w / q;
      b = w * w;

      // corrected SVF params, per Fons Adriaensen
      c1 = (a + b) / (1.f + a / 2.f + b / 4.f);
      c2 = b / (a + b);

      d0 = c1 * c2 / 4.f;
    }

    constexpr float lpStep(float in)
    {
      x = in - z1 - z2;
      z2 += c2 * z1;
      z1 += c1 * x;
      return d0 * x + z2;
    }

  private:
    float w{}, a{}, b{};
    float c1{}, c2{}, d0{};
    float z1{}, z2{}, x{};
  };

  SVF f1, f2;
  std::vector<float> lowpass;
  std::array<int16_t, 16384> ram;

  int16_t ai{0}, li{0}, acc{0};
  uint16_t ptr{0};
  uint16_t prog_offset{0};
  uint8_t current_program{20};

  // Sample rate for filters
  float sample_rate{48000.f};

  void prepare(halp::setup info) noexcept
  {
    sample_rate = info.rate;
    
    // Initialize filters
    f1.setFreq(5916.f, 0.6572f, sample_rate);
    f2.setFreq(9458.f, 2.536f, sample_rate);

    // Allocate buffers
    lowpass.resize(info.frames);
    ram.fill(0);

    // Clear buffers
    std::fill(lowpass.begin(), lowpass.end(), 0.f);
    std::fill(ram.begin(), ram.end(), 0);
  }

  void update_program(int prog)
  {
    current_program = prog;
    prog_offset = ((prog - 1) & 0x3f) << 7;
  }

  void operator()(int frames)
  {
    if(inputs.program != current_program)
      update_program(inputs.program);

    // Get audio pointers
    const double pos_mix = (inputs.mix);
    const double neg_mix = (1. - inputs.mix);
    const double* in_left = inputs.audio.samples[0];
    const double* in_right = inputs.audio.samples[1];
    double* out_left = outputs.audio.samples[0];
    double* out_right = outputs.audio.samples[1];

    // First pass: convert stereo to mono and apply input filters
    for (int i = 0; i < frames; i++)
    {
      float mono = static_cast<float>((in_left[i] + in_right[i]) / 2.0);
      lowpass[i] = f2.lpStep(f1.lpStep(mono));
    }

    // Process DSP engine (processing 2 samples at a time as per original)
    for (int i = 0; i < frames; i += 2)
    {
      // Ensure we don't go out of bounds on odd frame counts
      int samples_to_process = std::min(2, frames - i);

      // Run the DSP engine for each sample pair
      for (uint8_t step = 0; step < 128; step++)
      {
        uint16_t opcode = rom[prog_offset + step];
        
        switch (opcode & 0xc000)
        {
        case 0x0000:
          ai = ram[ptr];
          li = acc + (ai >> 1);
          break;
        case 0x4000:
          ai = ram[ptr];
          li = (ai >> 1);
          break;
        case 0x8000:
          ai = acc;
          ram[ptr] = ai;
          li = acc + (ai >> 1);
          break;
        case 0xc000:
          ai = acc;
          ram[ptr] = -ai;
          li = -(ai >> 1);
          break;
        }

        // Clamp
        if (ai > 2047)
          ai = 2047;
        if (ai < -2047)
          ai = -2047;

        if (step == 0x00)
        {
          // Load RAM from ADC
          ram[ptr] = static_cast<int16_t>(lowpass[i] * 2048);
        }
        else if (step == 0x60)
        {
          // Output right channel
          double sample = static_cast<double>(ai) / 2048.0;
          out_right[i] = pos_mix * sample + neg_mix * in_right[i];
          if (samples_to_process > 1)
            out_right[i + 1] = pos_mix * sample + neg_mix * in_right[i + 1];
        }
        else if (step == 0x70)
        {
          // Output left channel
          double sample = static_cast<double>(ai) / 2048.0;
          out_left[i] = pos_mix * sample + neg_mix * in_left[i];
          if (samples_to_process > 1)
            out_left[i + 1] = pos_mix * sample + neg_mix * in_left[i + 1];
        }
        else
        {
          // Everything else
          // ADC and DAC operations don't affect the accumulator
          // every other step ends with the accumulator latched from the Latch Input reg
          acc = li;
        }

        // 16kW of RAM
        ptr += opcode & 0x3fff;
        ptr &= 0x3fff;
      }
    }
  }
};

} // namespace examples
