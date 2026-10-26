#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>
#include <halp/audio.hpp>
#include <halp/grid_chooser.hpp>

#include <cstdint>
#include <optional>

namespace halp
{
//! An object's own period (e.g. its loop), for grid_unit::period, and the
//! origin of a free grid.
struct grid_period
{
  //! In frames. 0 or less: the object has no period.
  int64_t length{};
  //! Frames elapsed since the period's origin, at the first frame of the
  //! buffer. It may go past `length`: counted over every restart, multiples
  //! above 1 ("2x loop") fall on the same restarts from one buffer to the next;
  //! the phase within the period is enough for the others.
  int64_t position{};
};

namespace grid_detail
{
//! First point of a grid of `step` frames, `elapsed` frames past its origin
//! at the buffer start, that falls in [0; frames[.
inline std::optional<int>
next_frame_point(double step, double elapsed, int frames) noexcept
{
  if(!(step > 0.) || frames <= 0)
    return std::nullopt;
  constexpr double eps = 1e-9;
  const double k = std::ceil(elapsed / step - eps);
  const double rel = k * step - elapsed;
  if(rel >= double(frames))
    return std::nullopt;
  const double f = std::floor(rel + eps);
  return int(f < 0. ? 0. : f >= frames ? frames - 1 : f);
}
}

//! Where the next point of a grid falls in the buffer `tk` describes: the
//! frame offset of the first point in [0; tk.frames[, or nullopt when none
//! falls in this buffer. A grid that is none() is no quantization at all: 0,
//! now.
//!
//! - note: 4 x value quarters. Shorter than a bar it restarts at every bar line
//!   (a dotted quarter in 4/4 falls on 0, 1.5, 3, then the next bar); a bar or
//!   longer it counts from the last signature change.
//! - bars: value bars of the current signature, counted from the last
//!   signature change.
//! - free: value seconds, counted from the period's origin if the object has
//!   one, from the start of playback otherwise.
//! - period: value times the period, counted from its origin; nullopt
//!   without a period.
//! - parent: not resolved here; nullopt.
//!
//! The musical grids do not move while the transport is stopped or rewinding:
//! nullopt then. `Tick` is a halp::tick_musical or anything with its members.
//! No allocation, no lock: callable from the audio thread.
template <typename Tick>
std::optional<int> next_grid_point(
    const grid_value& g, const Tick& tk, double sample_rate,
    const grid_period& period = {}) noexcept
{
  if(g.none())
    return 0;
  if(!(g.value > 0.) || tk.frames <= 0)
    return std::nullopt;

  switch(g.unit)
  {
    case grid_unit::free: {
      if(!(sample_rate > 0.))
        return std::nullopt;
      const double elapsed
          = period.length > 0 ? double(period.position) : double(tk.position_in_frames);
      return grid_detail::next_frame_point(g.value * sample_rate, elapsed, tk.frames);
    }

    case grid_unit::period: {
      if(period.length <= 0)
        return std::nullopt;
      return grid_detail::next_frame_point(
          g.value * double(period.length), double(period.position), tk.frames);
    }

    case grid_unit::note:
    case grid_unit::bars: {
      const double lo = tk.start_position_in_quarters;
      const double hi = tk.end_position_in_quarters;
      if(!(hi > lo))
        return std::nullopt;

      const bool valid_sig = tk.signature.num > 0 && tk.signature.denom > 0;
      const double quarters_in_bar
          = valid_sig ? 4. * tk.signature.num / tk.signature.denom : 4.;
      const double unit
          = g.unit == grid_unit::note ? 4. * g.value : g.value * quarters_in_bar;
      if(!(unit > 0.))
        return std::nullopt;

      constexpr double eps = 1e-9;
      const auto frame_of = [&](double p) {
        const int64_t f = musical_to_frame(p, lo, hi, tk.frames);
        return int(f < 0 ? 0 : f >= tk.frames ? tk.frames - 1 : f);
      };

      if(g.unit == grid_unit::bars || unit >= quarters_in_bar - eps)
      {
        const double origin = tk.last_signature_change;
        const double k = std::ceil((lo - origin) / unit - eps);
        const double p = origin + k * unit;
        if(p >= hi)
          return std::nullopt;
        return frame_of(p);
      }

      // Shorter than a bar: from the bar line each point falls after
      double best = hi;
      tk.for_each_bar_segment(lo, hi, [&](double bar_line, double next_bar) {
        double k = std::ceil((lo - bar_line) / unit - eps);
        if(k < 0.)
          k = 0.;
        const double p = bar_line + k * unit;
        if(p < next_bar - eps && p < best)
          best = p;
      });
      if(best >= hi)
        return std::nullopt;
      return frame_of(best);
    }

    case grid_unit::parent:
      break;
  }
  return std::nullopt;
}
}
