#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstdint>

namespace avnd
{
/**
 * Host-independent transport snapshot for one processing block.
 *
 * Plug-in bindings translate their native transport structures into this
 * type and hand it to the process adapters; current_tick() then maps every
 * member the processor's own tick type declares (tempo, signature, musical
 * positions...) and ignores the rest.
 *
 * Units:
 *  - musical positions are in quarter notes;
 *  - flicks: 1 s == 705'600'000 flicks;
 *  - speed is 1 when the transport runs, 0 when stopped.
 */
struct transport_tick
{
  int32_t frames{};

  double tempo{120.};
  struct
  {
    uint16_t num{4};
    uint16_t denom{4};
  } signature;

  bool playing{};
  double speed{};

  int64_t position_in_frames{};
  double position_in_seconds{};
  double position_in_nanoseconds{};

  double start_position_in_quarters{};
  double end_position_in_quarters{};

  // Last bar boundary at (<=) the start, resp. the end, of the block.
  double bar_at_start{};
  double bar_at_end{};

  int64_t start_in_flicks{};
  int64_t end_in_flicks{};
};

inline constexpr double flicks_per_second = 705'600'000.;

// Last bar boundary <= pos, given a bar starting at bar_ref (all in quarters).
inline constexpr double
last_bar_before(double pos, double bar_ref, double quarters_per_bar) noexcept
{
  if(quarters_per_bar <= 0.)
    return bar_ref;
  const double bars = (pos - bar_ref) / quarters_per_bar;
  const auto whole = (int64_t)bars;
  const double base = bar_ref + double(whole - (bars < whole)) * quarters_per_bar;
  return base;
}
}
