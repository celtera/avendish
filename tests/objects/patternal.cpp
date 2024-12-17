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
    (Pattern) {42, {127, 127, 127, 127}}, // hi-hat
    (Pattern) {38, {0, 127, 0, 127}}, // snare
    (Pattern) {35, {127, 0, 127, 0}}, // bass drum
  };

  // Check that the input pattern is stored correctly:
  REQUIRE(patternalProcessor.inputs.patterns.value[0].note == 42);
  REQUIRE(patternalProcessor.inputs.patterns.value[0].pattern[0] == 127);
  REQUIRE(patternalProcessor.inputs.patterns.value[1].note == 38);
  REQUIRE(patternalProcessor.inputs.patterns.value[1].pattern[0] == 0);
  REQUIRE(patternalProcessor.inputs.patterns.value[2].note == 35);
  REQUIRE(patternalProcessor.inputs.patterns.value[2].pattern[0] == 127);
}

TEST_CASE("Compare two MIDI notes", "[advanced][patternal]")
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
  static constexpr double NS_PER_S = 1000000;

  Processor patternalProcessor;
  patternalProcessor.inputs.patterns.value = {
    (Pattern) {noteHiHat, {127, 127, 127, 127}}, // hi-hat
    (Pattern) {noteSnare, {0, 127, 0, 127}}, // snare
    (Pattern) {noteBassDrum, {127, 0, 127, 0}}, // bass drum
  };

  // Check the output on each tick:
  const int totalNumberOfTicks = (int) ticksPerSecond * testDuration;
  int64_t pos_in_frames = 0;
  double pos_in_ns = 0;
  for (int tickIndex = 0; tickIndex < totalNumberOfTicks; tickIndex++) {

    INFO("tick number " << tickIndex << " / " << totalNumberOfTicks);
    // INFO("Test tick " << tickIndex);
    pos_in_frames += bufferSize;
    pos_in_ns += tickDuration / NS_PER_S;
    tick_musical tk;
    // TODO tk.start_position_quarter = 
    // TODO tk.end_position_quarter = 
    tk.tempo = 120; // 120 BPM. One beat lasts 500 ms.
    tk.signature.num = 4;
    tk.signature.denom = 4;
    tk.frames = bufferSize;
    tk.position_in_nanoseconds = pos_in_ns;
    tk.position_in_frames = pos_in_frames;
    double timeNow = tickIndex * tickDuration;
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

    else {
      // FIXME: At some point, there should be some other notes.
      // INFO("tick number " << tickIndex << " out of " << totalNumberOfTicks);
      REQUIRE(patternalProcessor.outputs.midi.midi_messages.size() == 0);
    }
    // TODO: check the other ticks
  }

  // TODO check somethig with:
  // - start_position_quarter
  // - last_signature_change
}

