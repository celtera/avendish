#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/midi.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>
#include <ossia/network/dataspace/value_with_unit.hpp>
#include <ossia/network/dataspace/time.hpp>
#include <ossia/network/dataspace/gain.hpp>
#include <libremidi/message.hpp>

#include <avnd/concepts/midi_port.hpp>

namespace examples
{
/**
 * This example exhibits a simple, monophonic synthesizer.
 * It relies on some libossia niceties.
 */
struct Synth
{
  $(name, "My example synth");
  $(script_name, "synth_123");
  $(category, "Demo");
  $(author, "<AUTHOR>");
  $(description, "<DESCRIPTION>");
  $(uuid, "93eb0f78-3d97-4273-8a11-3df5714d66dc");

  struct {
    /** MIDI input: simply a list of timestamped messages.
     * Timestamp are in samples, 0 is the first sample.
     */
    avnd::midi_bus<"In"> midi;
  } inputs;

  struct {
    avnd::fixed_audio_bus<"Out", double, 2> audio;
  } outputs;

  struct conf
  {
    int sample_rate{44100};
  } configuration;

  void prepare(conf c) { configuration = c; }

  int in_flight = 0;
  ossia::frequency last_note{};
  ossia::linear last_volume{};
  double phase = 0.;

  /** Simple monophonic synthesizer **/
  void operator()(int frames)
  {
    // 1. Process the MIDI messages. We'll just play the latest note-on
    // in a not very sample-accurate way..

    for(auto& m : inputs.midi.midi_messages)
    {
      libremidi::message msg;
      msg.bytes.assign(m.bytes.begin(), m.bytes.end());
      // Let's ignore channels
      switch(msg.get_message_type()) {
        case libremidi::message_type::NOTE_ON:
          in_flight++;

          // Let's leverage the ossia unit conversion framework (adapted from Jamoma):
          // bytes is interpreted as a midi pitch and then converted to frequency.
          last_note = ossia::midi_pitch{msg.bytes[1]};

          // Store the velocity in linear gain
          last_volume = ossia::midigain{msg.bytes[2]};
          break;

        case libremidi::message_type::NOTE_OFF:
          in_flight--;
          break;
        default:
          break;
      }
    }

    // 2. Quit if we don't have any more note to play
    if(in_flight <= 0)
      return;

    // 3. Output some bleeps
    double increment = ossia::two_pi * last_note.dataspace_value / double(configuration.sample_rate);
    auto& out = outputs.audio.samples;

    for (int64_t j = 0; j < frames; j++)
    {
      out[0][j] = last_volume.dataspace_value * std::sin(phase);
      out[1][j] = out[0][j];

      phase += increment;
    }
  }
};
}
