/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

// Transport-to-tick conversion tests:
//  - clap_event_transport -> avnd::transport_tick (fixed-point beat/second
//    conversion, flag fallbacks, end-position computation);
//  - ProcessContext -> avnd::transport_tick through a structural mirror of
//    the SDK type (validity-flag fallbacks);
//  - transport_tick -> a processor's own tick type via current_tick.

#include <avnd/binding/clap/transport.hpp>
#include <avnd/binding/vst3/transport.hpp>
#include <avnd/wrappers/process_execution.hpp>
#include <avnd/wrappers/effect_container.hpp>

#include <catch2/catch_all.hpp>

TEST_CASE("clap: fixed-point beat conversion is exact", "[transport]")
{
  clap_event_transport_t tr{};
  tr.flags = CLAP_TRANSPORT_HAS_BEATS_TIMELINE | CLAP_TRANSPORT_HAS_TEMPO;
  tr.tempo = 120.;
  tr.song_pos_beats = (clap_beattime(5) << 31) + (clap_beattime(1) << 30); // 5.5
  auto t = avnd_clap::tick_from_transport(tr, 512, 48000.);
  REQUIRE(t.start_position_in_quarters == 5.5);
  REQUIRE(t.tempo == 120.);
}

TEST_CASE("clap: full transport", "[transport]")
{
  clap_event_transport_t tr{};
  tr.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE
             | CLAP_TRANSPORT_HAS_SECONDS_TIMELINE
             | CLAP_TRANSPORT_HAS_TIME_SIGNATURE | CLAP_TRANSPORT_IS_PLAYING;
  tr.tempo = 140.;
  tr.tsig_num = 3;
  tr.tsig_denom = 4;
  tr.song_pos_beats = clap_beattime(2.5 * CLAP_BEATTIME_FACTOR);
  tr.song_pos_seconds = clap_sectime(10.25 * CLAP_SECTIME_FACTOR);
  tr.bar_start = clap_beattime(1.5 * CLAP_BEATTIME_FACTOR);

  auto t = avnd_clap::tick_from_transport(tr, 24000, 48000.);
  REQUIRE(t.frames == 24000);
  REQUIRE(t.tempo == 140.);
  REQUIRE(t.signature.num == 3);
  REQUIRE(t.signature.denom == 4);
  REQUIRE(t.playing);
  REQUIRE(t.speed == 1.);
  REQUIRE(t.start_position_in_quarters == Catch::Approx(2.5));
  // end = start + frames * tempo / (60 * rate)
  REQUIRE(
      t.end_position_in_quarters
      == Catch::Approx(2.5 + 24000. * 140. / (60. * 48000.)));
  // Seconds timeline preferred.
  REQUIRE(t.position_in_seconds == Catch::Approx(10.25));
  REQUIRE(t.position_in_frames == 10.25 * 48000);
  // 3/4: bars are 3 quarters; end = 3.6667 -> last bar from 1.5 is 1.5.
  REQUIRE(t.bar_at_start == Catch::Approx(1.5));
  REQUIRE(t.bar_at_end == Catch::Approx(1.5));
  REQUIRE(
      t.start_in_flicks
      == std::llround(10.25 * avnd::flicks_per_second));
}

TEST_CASE("clap: bar_at_end advances across a bar boundary", "[transport]")
{
  clap_event_transport_t tr{};
  tr.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE
             | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
  tr.tempo = 120.;
  tr.tsig_num = 4;
  tr.tsig_denom = 4;
  tr.song_pos_beats = clap_beattime(3.9 * CLAP_BEATTIME_FACTOR);
  tr.bar_start = 0;

  // 0.2 quarters in this block: crosses the bar at 4.
  auto t = avnd_clap::tick_from_transport(tr, 4800, 48000.);
  REQUIRE(t.end_position_in_quarters == Catch::Approx(4.1));
  REQUIRE(t.bar_at_start == 0.);
  REQUIRE(t.bar_at_end == Catch::Approx(4.));
}

TEST_CASE("clap: absent flags fall back to defaults", "[transport]")
{
  clap_event_transport_t tr{};
  auto t = avnd_clap::tick_from_transport(tr, 4800, 48000.);
  REQUIRE(t.tempo == 120.);
  REQUIRE(t.signature.num == 4);
  REQUIRE(t.signature.denom == 4);
  REQUIRE(!t.playing);
  REQUIRE(t.speed == 0.);
  REQUIRE(t.start_position_in_quarters == 0.);
  // end still derived from the default tempo.
  REQUIRE(t.end_position_in_quarters == Catch::Approx(4800. * 2. / 48000.));
  REQUIRE(t.position_in_frames == 0);

  // steady_time is the last-resort frame position.
  auto t2 = avnd_clap::tick_from_transport(tr, 4800, 48000., 555);
  REQUIRE(t2.position_in_frames == 555);
}

TEST_CASE("clap: seconds derived from beats when absent", "[transport]")
{
  clap_event_transport_t tr{};
  tr.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE;
  tr.tempo = 90.;
  tr.song_pos_beats = clap_beattime(3. * CLAP_BEATTIME_FACTOR);
  auto t = avnd_clap::tick_from_transport(tr, 512, 48000.);
  REQUIRE(t.position_in_seconds == Catch::Approx(3. * 60. / 90.));
  REQUIRE(t.position_in_frames == std::llround(2. * 48000.));
}

// ---------------------------------------------------------------------------
// Structural mirror of Steinberg::Vst::ProcessContext (same field names and
// flag values; the converter is templated precisely so this compiles
// without the SDK).
struct FakeProcessContext
{
  enum StatesAndFlags : uint32_t
  {
    kPlaying = 1 << 1,
    kSystemTimeValid = 1 << 8,
    kProjectTimeMusicValid = 1 << 9,
    kTempoValid = 1 << 10,
    kBarPositionValid = 1 << 11,
    kTimeSigValid = 1 << 13,
  };
  uint32_t state{};
  double sampleRate{48000.};
  int64_t projectTimeSamples{};
  int64_t systemTime{};
  double projectTimeMusic{};
  double barPositionMusic{};
  double tempo{};
  int32_t timeSigNumerator{};
  int32_t timeSigDenominator{};
};

TEST_CASE("vst3: all fields valid", "[transport]")
{
  FakeProcessContext ctx;
  ctx.state = FakeProcessContext::kPlaying | FakeProcessContext::kTempoValid
              | FakeProcessContext::kProjectTimeMusicValid
              | FakeProcessContext::kBarPositionValid
              | FakeProcessContext::kTimeSigValid
              | FakeProcessContext::kSystemTimeValid;
  ctx.tempo = 140.;
  ctx.timeSigNumerator = 6;
  ctx.timeSigDenominator = 8;
  ctx.projectTimeMusic = 7.25;
  ctx.barPositionMusic = 6.;
  ctx.projectTimeSamples = 96000;
  ctx.systemTime = 123456789;

  auto t = stv3::tick_from_context(ctx, 4800, 44100.);
  REQUIRE(t.tempo == 140.);
  REQUIRE(t.signature.num == 6);
  REQUIRE(t.signature.denom == 8);
  REQUIRE(t.playing);
  REQUIRE(t.start_position_in_quarters == 7.25);
  REQUIRE(t.bar_at_start == 6.);
  REQUIRE(t.position_in_frames == 96000);
  REQUIRE(t.position_in_seconds == Catch::Approx(2.));
  REQUIRE(t.position_in_nanoseconds == 123456789.);
  REQUIRE(
      t.end_position_in_quarters
      == Catch::Approx(7.25 + 4800. * 140. / (60. * 48000.)));
}

TEST_CASE("vst3: musical position derived when only tempo is valid", "[transport]")
{
  FakeProcessContext ctx;
  ctx.state = FakeProcessContext::kTempoValid;
  ctx.tempo = 90.;
  ctx.projectTimeSamples = 48000; // 1 s at the context rate
  auto t = stv3::tick_from_context(ctx, 512, 44100.);
  REQUIRE(t.start_position_in_quarters == Catch::Approx(1.5));
  REQUIRE(t.signature.num == 4);
  REQUIRE(t.signature.denom == 4);
  // Bar fallback: bars aligned at 0 in 4/4 -> last bar before 1.5 is 0.
  REQUIRE(t.bar_at_start == 0.);
  REQUIRE(!t.playing);
}

TEST_CASE("vst3: nothing valid", "[transport]")
{
  FakeProcessContext ctx;
  ctx.projectTimeSamples = 24000; // 0.5 s
  auto t = stv3::tick_from_context(ctx, 512, 44100.);
  REQUIRE(t.tempo == 120.);
  REQUIRE(t.start_position_in_quarters == Catch::Approx(1.)); // 0.5 s at 120
  REQUIRE(t.position_in_nanoseconds == 0.);
}

TEST_CASE("vst3: bar position advances across the block", "[transport]")
{
  FakeProcessContext ctx;
  ctx.state = FakeProcessContext::kTempoValid
              | FakeProcessContext::kProjectTimeMusicValid
              | FakeProcessContext::kBarPositionValid
              | FakeProcessContext::kTimeSigValid;
  ctx.tempo = 120.;
  ctx.timeSigNumerator = 4;
  ctx.timeSigDenominator = 4;
  ctx.projectTimeMusic = 3.9;
  ctx.barPositionMusic = 0.;
  auto t = stv3::tick_from_context(ctx, 4800, 48000.);
  REQUIRE(t.end_position_in_quarters == Catch::Approx(4.1));
  REQUIRE(t.bar_at_end == Catch::Approx(4.));
}

// ---------------------------------------------------------------------------
// End to end: the transport tick lands in a processor's own tick type.
struct TickProcessor
{
  static consteval auto name() { return "TickProcessor"; }
  struct
  {
  } inputs;
  struct
  {
  } outputs;

  struct tick
  {
    int frames{};
    double tempo{};
    struct
    {
      int num{}, denom{};
    } signature;
    double start_position_in_quarters{};
    double end_position_in_quarters{};
    bool playing{};
  };

  tick last{};
  void operator()(tick t) { last = t; }
};

TEST_CASE("transport tick maps onto the processor's tick", "[transport]")
{
  avnd::effect_container<TickProcessor> fx;

  avnd::transport_tick tr;
  tr.frames = 256;
  tr.tempo = 133.;
  tr.signature.num = 7;
  tr.signature.denom = 8;
  tr.start_position_in_quarters = 11.5;
  tr.end_position_in_quarters = 11.75;
  tr.playing = true;

  auto t = avnd::get_tick_or_frames(fx, tr);
  avnd::invoke_effect(fx, t);

  REQUIRE(fx.effect.last.frames == 256);
  REQUIRE(fx.effect.last.tempo == 133.);
  REQUIRE(fx.effect.last.signature.num == 7);
  REQUIRE(fx.effect.last.signature.denom == 8);
  REQUIRE(fx.effect.last.start_position_in_quarters == 11.5);
  REQUIRE(fx.effect.last.end_position_in_quarters == 11.75);
  REQUIRE(fx.effect.last.playing);
}
