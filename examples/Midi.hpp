#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>

#include <array>
#include <cstddef>
#include <cstdint>

namespace examples
{
/**
 * A very basic example of polyphonic MIDI synthesizer
 */
struct Midi
{
  static consteval auto name() { return "MIDI example"; }
  static consteval auto c_name() { return "avnd_midi"; }
  static consteval auto uuid()
  {
    return "8e2d87fc-dd8f-4e70-9b71-8a977d8aa58b";
  }

  struct
  {
    struct
    {
      static consteval auto name() { return "Input"; }
      double** samples;
      int channels;
    } audio;

    // Our MIDI input port.
    struct
    {
      static consteval auto name() { return "MIDI"; }
      struct
      {
        const uint8_t* bytes{};
        std::size_t size{};
        int timestamp{}; // relative to the beginning of the tick
      } * midi_messages{};
      std::size_t size{};
    } midi_unsafe;
  } inputs;

  struct
  {
    struct
    {
      static consteval auto name() { return "Output"; }
      double** samples;
    } audio;
  } outputs;

  struct voice
  {
    int velocity{0};
    double phase{0.};
  };

  std::array<voice, 128> notes;
  double gain = 1.;

  struct conf
  {
    int sample_rate{44100};
  } configuration;

  void prepare(conf c) { configuration = c; }

  void operator()(int N)
  {
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;

    const auto chans = p1.channels;

    for (int i = 0; i < inputs.midi_unsafe.size; i++)
    {
      auto& m = inputs.midi_unsafe.midi_messages[i];
      if (m.bytes[0] == 144)
      {
        notes[m.bytes[1]].velocity = m.bytes[2];
        notes[m.bytes[1]].phase = 0.;
        gain = 1.;
      }
      else
      {
        notes[m.bytes[1]].velocity = 0;
      }
    }

    // Zero our buffer
    for (int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      for (int j = 0; j < N; j++)
      {
        out[j] = 0.;
      }
    }

    // Process notes
    double g{};
    for (int note = 0; note < notes.size(); ++note)
    {
      auto& [velocity, note_phase] = notes[note];
      if (velocity == 0)
        continue;

      const double freq = std::pow(2., (note - 69.) / 12.) * 440.;
      const double phase_increment = 2 * 3.14 * freq / 44100;

      const double init_phase = note_phase;
      // Process the input buffer
      for (int i = 0; i < chans; i++)
      {
        g = gain;
        double phase = init_phase;
        auto& in = p1.samples[i];
        auto& out = p2.samples[i];

        for (int j = 0; j < N; j++)
        {
          out[j]
              += std::sin(in[j] + phase) * (velocity / 127.) * (g *= 0.99995);
          phase += phase_increment;
        }
        note_phase = phase;
        // TODO put the phases back in [0; 2pi]
      }
    }
    gain = g;
  }
};
}
