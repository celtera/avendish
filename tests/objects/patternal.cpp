#include <cmath> // std::fmod
#include <algorithm> // std::max
#include <catch2/catch_all.hpp>

#include <halp/audio.hpp>
#include <halp/midi.hpp>
#include <examples/Advanced/Patternal/Patternal.hpp>
#include <examples/Advanced/Patternal/Patternal.hpp>

using Processor = patternal::Processor;
using Pattern = patternal::Pattern;
using tick_musical = halp::tick_musical;
using midi_msg = halp::midi_msg;

static const int noteHiHat = 42;
static const int noteSnare = 38;
static const int noteBassDrum = 35;
static const uint8_t messageNoteOn = 0x90; // on the first channel
static const uint8_t messageNoteOff = 0x80; // on the first channel
static const uint8_t velocityZero = 0;
static const uint8_t velocityFull = 127;

/**
 * Create a MIDI note message.
 */
void makeNote(midi_msg& ret, uint8_t note, uint8_t velocity, int64_t timestamp) {
  bool isNoteOn = velocity > 0;
  if (isNoteOn) {
    ret.bytes = { messageNoteOn, note, velocity};
  } else {
    ret.bytes = { messageNoteOff, note, velocity};
  }
  ret.timestamp = timestamp;
}

// Tests for the Patternal object
TEST_CASE("The input pattern is stored correctly", "[advanced][patternal]")
{
  // Instanciate the object:
  Processor patternalProcessor;

  // Create the input pattern:
  patternalProcessor.inputs.patterns.value = {
    {42, {127, 127, 127, 127}}, // hi-hat
    {38, {0, 127, 0, 127}}, // snare
    {35, {127, 0, 127, 0}}, // bass drum
  };

  // Check that the input pattern is stored correctly:
  REQUIRE(patternalProcessor.inputs.patterns.value[0].note == 42);
  REQUIRE(patternalProcessor.inputs.patterns.value[0].pattern[0] == 127);
  REQUIRE(patternalProcessor.inputs.patterns.value[1].note == 38);
  REQUIRE(patternalProcessor.inputs.patterns.value[1].pattern[0] == 0);
  REQUIRE(patternalProcessor.inputs.patterns.value[2].note == 35);
  REQUIRE(patternalProcessor.inputs.patterns.value[2].pattern[0] == 127);
}

TEST_CASE("Compare two MIDI notes", "[MIDI]")
{
  // Test that comparing two notes works:
  midi_msg simple1;
  midi_msg simple2;
  makeNote(simple1, 60, 127, 0);
  makeNote(simple2, 60, 127, 0);
  REQUIRE(simple1 == simple2);
}

TEST_CASE("MIDI messages are output properly", "[advanced][patternal]")
{
  // Our sampling rate is 48kHz and the buffer size is 512
  static constexpr double samplingRate = 48000;
  static constexpr int bufferSize = 512;
  static constexpr double tickDuration = samplingRate / bufferSize; // seconds
  static constexpr double ticksPerSecond = 1.0 / tickDuration;
  static constexpr double testDuration = 3.0; // seconds
  static constexpr double NS_PER_S = 1000000000;
  static constexpr double durationOfAQuarter = 0.5; // seconds
  static constexpr int quartersPerBar = 4;

  Processor patternalProcessor;
  patternalProcessor.inputs.patterns.value = {
    {noteHiHat, {127, 127, 127, 127}}, // hi-hat
    {noteSnare, {0, 127, 0, 127}}, // snare
    {noteBassDrum, {127, 0, 127, 0}}, // bass drum
  };

  // Check the output on each tick:
  const int totalNumberOfTicks = (int) ticksPerSecond * testDuration;
  int64_t posInFrames = 0;
  double posInNs = 0;
  double previousMaxQuarter = 0; // The previous max quarter number during the previous tick.

  for (int tickIndex = 0; tickIndex < totalNumberOfTicks; tickIndex++) {
    // Debug info
    INFO("tick number " << tickIndex << " / " << totalNumberOfTicks);

    // Calculate stuff:
    int shouldPlayBeat = -1; // Index of the beat we should play now, or -1 if none.
    // INFO("Test tick " << tickIndex);
    posInFrames += bufferSize;
    posInNs += tickDuration / NS_PER_S;
    double timeNow = tickIndex * tickDuration;
    double timeAtEndOfTick = (tickIndex + 1) * tickDuration;
    double startPosInQuarters = timeNow / durationOfAQuarter;
    double endPosInQuarters = (timeNow + tickDuration) / durationOfAQuarter;
    double barAtStart = std::fmod(startPosInQuarters, quartersPerBar);
    double barAtEnd = std::fmod(endPosInQuarters, quartersPerBar);
    double currentMaxQuarter = std::max(startPosInQuarters, endPosInQuarters);

    // Populate the tick_musical:
    tick_musical tk;
    tk.tempo = 120; // 120 BPM. One beat lasts 500 ms.
    tk.signature.num = 4;
    tk.signature.denom = quartersPerBar;
    tk.frames = bufferSize;
    tk.position_in_nanoseconds = posInNs; // We don't really need to set this pos in ns.
    tk.position_in_frames = posInFrames;
    tk.last_signature_change = 0; // The signature never changes.
    tk.start_position_in_quarters = startPosInQuarters;
    tk.end_position_in_quarters = endPosInQuarters;
    tk.bar_at_start = barAtStart;
    tk.bar_at_end = barAtEnd;

    // Process it:
    patternalProcessor(tk);

    // Test the 1st tick:
    if (tickIndex == 0) {
      // FIXME: A note with velocity 0 is sent right away, immediately after each note on.
      // How long should the notes last?
      // TODO: Consider making the note last until the next beat.
      REQUIRE(patternalProcessor.outputs.midi.midi_messages.size() == 4);
      // Hi-hat note on:
      midi_msg expectedNote0;
      makeNote(expectedNote0, noteHiHat, velocityFull, -2147483648); // FIXME: Why this timestamp?
      REQUIRE(patternalProcessor.outputs.midi.midi_messages[0] == expectedNote0);
      // A hi-hat note off:
      midi_msg expectedNote1;
      makeNote(expectedNote1, noteHiHat, velocityZero, -2147483648);
      REQUIRE(patternalProcessor.outputs.midi.midi_messages[1] == expectedNote1);
      // Bass drum note on:
      midi_msg expectedNote2;
      makeNote(expectedNote2, noteBassDrum, velocityFull, -2147483648);
      REQUIRE(patternalProcessor.outputs.midi.midi_messages[2] == expectedNote2);
      // A bass drum note off:
      midi_msg expectedNote3;
      makeNote(expectedNote3, noteBassDrum, velocityZero, -2147483648);
      REQUIRE(patternalProcessor.outputs.midi.midi_messages[3] == expectedNote3);
      // Note: There is no snare note to output, since it's off, for now.
    }

    // Test the 2nd tick:
    else if (tickIndex == 1) {
      // There should be no MIDI messages in the output on this tick.
      REQUIRE(patternalProcessor.outputs.midi.midi_messages.size() == 0);
    }

    else if (currentMaxQuarter != previousMaxQuarter) {
      // We assume there won't be two new quarters within the same buffer - at 120 BPM
      INFO("quarter number: " << currentMaxQuarter);
      if (currentMaxQuarter == 1) {
        REQUIRE(patternalProcessor.outputs.midi.midi_messages.size() == 4);
        // A hi-hat note off:
        midi_msg note0;
        makeNote(note0, noteHiHat, velocityZero, -2147483648);
        REQUIRE(patternalProcessor.outputs.midi.midi_messages[0] == note0);
        // Hi-hat note on:
        midi_msg note1;
        makeNote(note1, noteHiHat, velocityFull, -2147483648);
        REQUIRE(patternalProcessor.outputs.midi.midi_messages[1] == note1);
        // Snare note on:
        midi_msg note2;
        makeNote(note2, noteSnare, velocityFull, -2147483648);
        REQUIRE(patternalProcessor.outputs.midi.midi_messages[2] == note2);
        // A bass drum note off:
        midi_msg note3;
        makeNote(note3, noteBassDrum, velocityZero, -2147483648);
        REQUIRE(patternalProcessor.outputs.midi.midi_messages[3] == note3);
      }
    } else {
      REQUIRE(patternalProcessor.outputs.midi.midi_messages.size() == 0);
    }
    // FIXME: At some point, there should be some other notes.
    // TODO: check the other ticks

    // Last operations before the next tick:
    previousMaxQuarter = currentMaxQuarter;
  }
}

