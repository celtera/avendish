#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/transport_tick.hpp>

#include <cmath>
#include <cstdint>

namespace stv3
{
/**
 * Translate a ProcessContext into an avnd::transport_tick for one block.
 *
 * Templated on the context type so the conversion is testable without the
 * SDK headers: any type with ProcessContext's fields and StatesAndFlags
 * constants works.
 *
 * Semantics:
 *  - validity flags are honored per field: tempo (kTempoValid, else 120),
 *    signature (kTimeSigValid, else 4/4), musical position
 *    (kProjectTimeMusicValid, else derived from projectTimeSamples at the
 *    current tempo), bar position (kBarPositionValid, else the last bar
 *    boundary before the start assuming bars aligned at 0);
 *  - projectTimeSamples needs no flag (always provided per the spec) and
 *    yields the seconds/frames/flicks positions;
 *  - position_in_nanoseconds comes from systemTime when kSystemTimeValid;
 *  - end_position_in_quarters = start + frames * tempo / (60 * rate).
 */
template <typename ProcessContext>
inline avnd::transport_tick
tick_from_context(const ProcessContext& ctx, int32_t frames, double fallback_rate) noexcept
{
  avnd::transport_tick t;
  t.frames = frames;

  if(ctx.state & ProcessContext::kTempoValid)
    t.tempo = ctx.tempo;
  if(ctx.state & ProcessContext::kTimeSigValid)
  {
    t.signature.num = uint16_t(ctx.timeSigNumerator);
    t.signature.denom = uint16_t(ctx.timeSigDenominator);
  }
  t.playing = (ctx.state & ProcessContext::kPlaying) != 0;
  t.speed = t.playing ? 1. : 0.;

  const double rate = ctx.sampleRate > 0 ? ctx.sampleRate : fallback_rate;
  const double seconds = rate > 0 ? double(ctx.projectTimeSamples) / rate : 0.;

  t.position_in_frames = ctx.projectTimeSamples;
  t.position_in_seconds = seconds;
  t.start_in_flicks = int64_t(std::llround(seconds * avnd::flicks_per_second));
  t.end_in_flicks = int64_t(std::llround(
      (seconds + (rate > 0 ? double(frames) / rate : 0.))
      * avnd::flicks_per_second));

  if(ctx.state & ProcessContext::kSystemTimeValid)
    t.position_in_nanoseconds = double(ctx.systemTime);

  if(ctx.state & ProcessContext::kProjectTimeMusicValid)
    t.start_position_in_quarters = ctx.projectTimeMusic;
  else
    t.start_position_in_quarters = seconds * t.tempo / 60.;

  t.end_position_in_quarters = t.start_position_in_quarters
                               + (rate > 0 ? double(frames) * t.tempo / (60. * rate) : 0.);

  const double quarters_per_bar
      = t.signature.denom > 0
            ? double(t.signature.num) * 4. / double(t.signature.denom)
            : 4.;
  if(ctx.state & ProcessContext::kBarPositionValid)
    t.bar_at_start = ctx.barPositionMusic;
  else
    t.bar_at_start
        = avnd::last_bar_before(t.start_position_in_quarters, 0., quarters_per_bar);
  t.bar_at_end = avnd::last_bar_before(
      t.end_position_in_quarters, t.bar_at_start, quarters_per_bar);

  return t;
}
}
