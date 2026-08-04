#include <catch2/catch_all.hpp>

#include <halp/audio.hpp>

namespace
{
halp::tick_musical
musical_tick(int frames, double start_quarters, double end_quarters, double bar = 0.)
{
  halp::tick_musical t;
  t.frames = frames;
  t.tempo = 120.;
  t.signature = {4, 4};
  t.position_in_frames = 0;
  t.start_position_in_quarters = start_quarters;
  t.end_position_in_quarters = end_quarters;
  t.last_signature_change = 0.;
  t.bar_at_start = bar;
  t.bar_at_end = bar;
  return t;
}
}

TEST_CASE("quantification: forward sixteenths", "[audio][quantification]")
{
  // One quarter note across 64 frames, sixteenth-note rate: four points.
  const auto t = musical_tick(64, 0., 1.);
  const auto r = t.get_quantification_date(16.);

  REQUIRE(r.size() == 4);
  CHECK(r[0] == std::pair{0, 0});
  CHECK(r[1] == std::pair{16, 1});
  CHECK(r[2] == std::pair{32, 2});
  CHECK(r[3] == std::pair{48, 3});
}

TEST_CASE("quantification: rewinding walks the same points backwards",
          "[audio][quantification]")
{
  // The mirror of the tick above. Four subdivisions are crossed, in the
  // opposite musical order, and their buffer positions still increase.
  //
  // A tick reports the point on its own start and leaves the one on its end to
  // whatever runs next: forward that is [start; end[, rewinding ]end; start].
  // So this one reports quarter 1, where it begins, and leaves quarter 0 out.
  const auto t = musical_tick(64, 1., 0.);
  const auto r = t.get_quantification_date(16.);

  REQUIRE(r.size() == 4);
  CHECK(r[0] == std::pair{0, 4});
  CHECK(r[1] == std::pair{16, 3});
  CHECK(r[2] == std::pair{32, 2});
  CHECK(r[3] == std::pair{48, 1});

  int prev = -1;
  for(auto [frame, index] : r)
  {
    CHECK(frame > prev);
    CHECK(frame >= 0);
    CHECK(frame < t.frames);
    prev = frame;
  }
}

TEST_CASE("quantification: a rewinding tick shorter than the rate is silent",
          "[audio][quantification]")
{
  // Used to report one point at frame 0 on every buffer, whatever the rate:
  // a step sequencer driven by this fired once per buffer while rewinding.
  const auto t = musical_tick(64, 0.95, 0.85);
  CHECK(t.get_quantification_date(4.).empty());

  const auto fwd = musical_tick(64, 0.85, 0.95);
  CHECK(fwd.get_quantification_date(4.).empty());
}

TEST_CASE("quantification: rewinding over a bar", "[audio][quantification]")
{
  // Four quarters backwards over a whole bar at the quarter rate.
  auto t = musical_tick(64, 4., 0.);
  t.bar_at_start = 4.;
  t.bar_at_end = 0.;

  const auto r = t.get_quantification_date(4.);

  REQUIRE(!r.empty());
  int prev = -1;
  for(auto [frame, index] : r)
  {
    CHECK(frame > prev);
    CHECK(frame >= 0);
    CHECK(frame < t.frames);
    prev = frame;
  }
}

TEST_CASE("quantification: points come out in buffer order", "[audio][quantification]")
{
  // A bar boundary inside the tick is appended to the result after the
  // subdivisions that precede it in the buffer. Consumers walk the list in
  // order - several stop at the first entry - so it has to be sorted.
  // A whole bar, ending exactly on the next bar line: the boundary lands on
  // the last frame but is appended before the four quarters that precede it.
  auto t = musical_tick(64, 0., 4.);
  t.bar_at_start = 0.;
  t.bar_at_end = 4.;

  const auto r = t.get_quantification_date(4.);

  REQUIRE(r.size() >= 2);
  int prev = -1;
  for(auto [frame, index] : r)
  {
    CHECK(frame >= prev);
    CHECK(frame >= 0);
    CHECK(frame < t.frames);
    prev = frame;
  }
}

TEST_CASE("quantification: degenerate ticks", "[audio][quantification]")
{
  // No musical duration: the single point at frame 0 is what objects driven
  // with the musical fields left at zero rely on.
  const auto flat = musical_tick(64, 1., 1.);
  const auto r = flat.get_quantification_date(16.);
  REQUIRE(r.size() == 1);
  CHECK(r[0] == std::pair{0, 0});

  // No frames at all.
  const auto empty = musical_tick(0, 0., 1.);
  CHECK(empty.get_quantification_date(16.).empty());

  // A rate of zero means "every tick".
  const auto zero_rate = musical_tick(64, 1., 0.);
  const auto zr = zero_rate.get_quantification_date(0.);
  REQUIRE(zr.size() == 1);
  CHECK(zr[0] == std::pair{0, 0});
}

TEST_CASE("quantification: every reported frame is inside the buffer",
          "[audio][quantification]")
{
  for(double rate : {1., 2., 4., 8., 16., 32.})
  {
    for(int frames : {1, 7, 64, 512})
    {
      for(double span : {0.1, 1., 4., 17.})
      {
        const auto fwd = musical_tick(frames, 0., span);
        const auto bwd = musical_tick(frames, span, 0.);
        CAPTURE(rate, frames, span);

        for(auto [frame, index] : fwd.get_quantification_date(rate))
        {
          CHECK(frame >= 0);
          CHECK(frame < frames);
        }
        for(auto [frame, index] : bwd.get_quantification_date(rate))
        {
          CHECK(frame >= 0);
          CHECK(frame < frames);
        }
      }
    }
  }
}

TEST_CASE("quantification: pos_to_frame stays inside the buffer",
          "[audio][quantification]")
{
  const auto fwd = musical_tick(64, 0., 1.);
  CHECK(fwd.pos_to_frame(0.) == 0);
  CHECK(fwd.pos_to_frame(0.5) == 32);

  // Rewinding, the musical position moves down as the buffer advances.
  const auto bwd = musical_tick(64, 1., 0.);
  CHECK(bwd.pos_to_frame(1.) == 0);
  CHECK(bwd.pos_to_frame(0.5) == 32);

  // A tick with no musical duration used to divide by zero here and convert a
  // NaN to int.
  const auto flat = musical_tick(64, 1., 1.);
  CHECK(flat.pos_to_frame(1.) == 0);
  CHECK(flat.pos_to_frame(9999.) == 0);

  // Out-of-tick positions clamp into the buffer rather than indexing past it.
  CHECK(fwd.pos_to_frame(-100.) == 0);
  CHECK(fwd.pos_to_frame(100.) == 63);

  const auto no_frames = musical_tick(0, 0., 1.);
  CHECK(no_frames.pos_to_frame(0.5) == 0);
}
