#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/transport_tick.hpp>
#include <clap/all.h>

#include <cmath>

namespace avnd_clap
{
/**
 * Translate a clap_event_transport into an avnd::transport_tick for one
 * block of `frames` frames at `sample_rate`.
 *
 * Semantics:
 *  - clap_beattime / clap_sectime are 32.31 fixed point; conversion is a
 *    plain division by CLAP_BEATTIME_FACTOR / CLAP_SECTIME_FACTOR (exact in
 *    double for any position below ~2^21 beats / seconds). Beats are
 *    quarter notes.
 *  - Fields whose flag is absent fall back to: tempo 120, signature 4/4,
 *    positions 0.
 *  - end_position_in_quarters = start + frames * tempo / (60 * rate),
 *    i.e. tempo is taken as constant over the block (tempo_inc is applied
 *    by hosts through per-block transport updates).
 *  - The seconds timeline is preferred for the seconds/frames/flicks
 *    positions; without it they are derived from the beats timeline at
 *    constant tempo; without either they stay at steady_time (if provided).
 */
inline avnd::transport_tick tick_from_transport(
    const clap_event_transport_t& tr, uint32_t frames, double sample_rate,
    int64_t steady_time = -1) noexcept
{
  avnd::transport_tick t;
  t.frames = int32_t(frames);

  if(tr.flags & CLAP_TRANSPORT_HAS_TEMPO)
    t.tempo = tr.tempo;
  if(tr.flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE)
  {
    t.signature.num = tr.tsig_num;
    t.signature.denom = tr.tsig_denom;
  }
  t.playing = (tr.flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
  t.speed = t.playing ? 1. : 0.;

  const double rate = sample_rate > 0 ? sample_rate : 44100.;
  const double block_quarters = double(frames) * t.tempo / (60. * rate);

  if(tr.flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE)
  {
    t.start_position_in_quarters
        = double(tr.song_pos_beats) / CLAP_BEATTIME_FACTOR;
    t.end_position_in_quarters = t.start_position_in_quarters + block_quarters;

    const double bar_start = double(tr.bar_start) / CLAP_BEATTIME_FACTOR;
    const double quarters_per_bar
        = t.signature.denom > 0
              ? double(t.signature.num) * 4. / double(t.signature.denom)
              : 4.;
    t.bar_at_start = bar_start;
    t.bar_at_end = avnd::last_bar_before(
        t.end_position_in_quarters, bar_start, quarters_per_bar);
  }
  else
  {
    t.end_position_in_quarters = block_quarters;
  }

  double seconds = 0.;
  bool has_seconds = false;
  if(tr.flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE)
  {
    seconds = double(tr.song_pos_seconds) / CLAP_SECTIME_FACTOR;
    has_seconds = true;
  }
  else if(tr.flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE)
  {
    // Constant-tempo approximation of the seconds position.
    seconds = t.start_position_in_quarters * 60. / t.tempo;
    has_seconds = true;
  }

  if(has_seconds)
  {
    t.position_in_seconds = seconds;
    t.position_in_frames = int64_t(std::llround(seconds * rate));
    t.start_in_flicks = int64_t(std::llround(seconds * avnd::flicks_per_second));
    t.end_in_flicks = int64_t(std::llround(
        (seconds + double(frames) / rate) * avnd::flicks_per_second));
  }
  else if(steady_time >= 0)
  {
    t.position_in_frames = steady_time;
    t.position_in_seconds = double(steady_time) / rate;
  }

  return t;
}
}
