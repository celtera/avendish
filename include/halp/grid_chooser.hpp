#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <cstdint>
#include <string_view>

namespace halp
{
//! What a grid value counts in. The numbers are part of the stored value (the
//! second component of the host's {value, unit} pair) and must not change.
enum class grid_unit : int8_t
{
  //! The grid of the parent (reserved, for scenarios).
  parent = -1,
  //! Seconds. 0 is no grid at all ("None").
  free = 0,
  //! A fraction of a whole note: 1 is a whole note (four quarters, whatever
  //! the signature), 0.25 a quarter, 1/12 an eighth triplet, 3/16 a dotted
  //! eighth, 2 two whole notes. Never a bar.
  note = 1,
  //! Whole bars of the current signature.
  bars = 2,
  //! A multiple of the object's own period, e.g. its loop: 1 is every
  //! restart, 2 every other one, 1/2 twice per period.
  period = 3,
};

//! A grid, as the object receives it: the value is not converted to seconds,
//! see halp/grid.hpp to find where its points fall.
struct grid_value
{
  grid_unit unit{grid_unit::free};
  double value{};
  //! Reserved: a shift of the grid, in the same unit. Nothing reads it yet.
  double offset{};

  //! No quantization.
  constexpr bool none() const noexcept
  {
    return unit == grid_unit::free && !(value > 0.);
  }

  static constexpr grid_value seconds(double s) noexcept
  {
    return {grid_unit::free, s, 0.};
  }
  static constexpr grid_value note(double whole_note_fraction) noexcept
  {
    return {grid_unit::note, whole_note_fraction, 0.};
  }
  static constexpr grid_value bars(double count) noexcept
  {
    return {grid_unit::bars, count, 0.};
  }
  static constexpr grid_value period(double multiple) noexcept
  {
    return {grid_unit::period, multiple, 0.};
  }

  //! What a halp::time_chooser held: a fraction of a whole note when synced,
  //! seconds otherwise. A migration whose synced values meant bars rather
  //! than whole notes uses bars() for those.
  static constexpr grid_value from_time_chooser(double value, bool sync) noexcept
  {
    return sync ? note(value) : seconds(value);
  }

  constexpr bool operator==(const grid_value&) const noexcept = default;
};

//! The units a grid chooser offers, or-ed together.
namespace grid_units
{
inline constexpr uint32_t free = 1u << 0;
inline constexpr uint32_t note = 1u << 1;
inline constexpr uint32_t bars = 1u << 2;
inline constexpr uint32_t period = 1u << 3;
inline constexpr uint32_t parent = 1u << 4;
inline constexpr uint32_t musical = free | note | bars;
}

constexpr uint32_t grid_unit_flag(grid_unit u) noexcept
{
  switch(u)
  {
    case grid_unit::free:
      return grid_units::free;
    case grid_unit::note:
      return grid_units::note;
    case grid_unit::bars:
      return grid_units::bars;
    case grid_unit::period:
      return grid_units::period;
    case grid_unit::parent:
      return grid_units::parent;
  }
  return 0;
}

struct grid_setup
{
  //! Which units the host lets the user pick, see grid_units.
  uint32_t units = grid_units::musical;
  grid_value init = grid_value::note(0.25);
  //! The longest grid in bars.
  int max_bars = 16;
  //! The longest free time, in seconds.
  double max_seconds = 8.;
};

/// Grid chooser ///
//! A quantization grid: free seconds, a note value, a number of bars or a
//! multiple of the object's own period, as allowed by `setup.units`.
//! `period` names the period in the host's labels ("loop": "2x loop").
template <
    static_string lit, grid_setup setup = grid_setup{}, static_string period = "loop">
struct grid_chooser
{
  enum class widget
  {
    grid_chooser
  };

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  static constexpr grid_setup grid() noexcept { return setup; }
  static constexpr std::string_view period_name() noexcept
  {
    return std::string_view{period.value};
  }

  grid_value value = setup.init;

  operator grid_value&() noexcept { return value; }
  operator const grid_value&() const noexcept { return value; }
  auto& operator=(grid_value t) noexcept
  {
    value = t;
    return *this;
  }
};
}
