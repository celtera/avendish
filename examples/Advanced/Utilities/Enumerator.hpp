#pragma once
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace ao
{
/**
 * @brief Walks through a list, one value at a time.
 *
 * A bang on Trigger outputs the next value of the list (an internal counter,
 * bounded by clip, wrap or fold); a number on Trigger outputs the value at that
 * index. Or the next value goes out by itself, at every tick or at a given
 * interval.
 */
struct Enumerator
{
  halp_meta(name, "Enumerator")
  halp_meta(c_name, "avnd_enumerator")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Outputs the values of a list one by one: the next one on each bang, the "
      "one at an index on a number, or automatically at each tick or interval")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/array-utilities.html#enumerator")
  halp_meta(uuid, "7e998d33-864d-4483-83f4-a9db48e5703f")

  enum Mode
  {
    Manual,
    EveryTick,
    Timed
  };
  enum Bounds
  {
    Clip,
    Wrap,
    Fold
  };

  struct
  {
    // An event port: a bang steps, a number jumps to that index.
    halp::val_port<"Trigger", std::optional<ossia::value>> trigger;
    // A list, or a vec2f / vec3f / vec4f (their components). Kept until the
    // next one: a list usually arrives once, when it changes.
    halp::val_port<"List", std::optional<ossia::value>> list;
    halp::combobox_t<"Mode", Mode> mode;
    halp::combobox_t<"Bounds", Bounds> bounds;
    halp::time_chooser<"Interval", halp::range{0.001, 60., 0.5}> interval;
  } inputs;

  struct
  {
    halp::val_port<"Value", std::optional<ossia::value>> value;
    // Sent when the index changes
    halp::val_port<"Index", std::optional<int>> index;
  } outputs;

  void prepare(halp::setup info) noexcept
  {
    if(info.rate > 0)
      m_rate = info.rate;
  }

  //! The list the List port holds, as values.
  static std::vector<ossia::value> items(const ossia::value& v)
  {
    if(auto l = v.target<std::vector<ossia::value>>())
      return *l;
    if(v.target<ossia::vec2f>() || v.target<ossia::vec3f>() || v.target<ossia::vec4f>())
      return ossia::convert<std::vector<ossia::value>>(v);
    if(v.valid())
      return {v};
    return {};
  }

  //! Maps the counter onto [0, n) with the Bounds mode.
  static int shape(int64_t pos, int n, Bounds b) noexcept
  {
    if(n <= 0)
      return 0;
    switch(b)
    {
      default:
      case Clip:
        return int(std::clamp<int64_t>(pos, 0, n - 1));
      case Wrap:
        return int(((pos % n) + n) % n);
      case Fold: {
        if(n == 1)
          return 0;
        const int64_t period = 2 * (n - 1);
        const int64_t p = ((pos % period) + period) % period;
        return int(p < n ? p : period - p);
      }
    }
  }

  // Without it, the bindings do not know operator() takes a tick and never
  // call it.
  using tick = halp::tick;
  void operator()(halp::tick t)
  {
    outputs.value.value.reset();
    outputs.index.value.reset();

    if(auto& l = inputs.list.value)
    {
      m_list = items(*l);
      l.reset();
    }
    const auto& list = m_list;
    const int n = int(list.size());

    bool emit = false;
    if(auto& trig = inputs.trigger.value)
    {
      if(trig->target<ossia::impulse>())
      {
        step();
        emit = true;
      }
      else if(trig->valid())
      {
        // A number: that index (a clipped, wrapped or folded one). Bounded so
        // that the rounding and the Wrap / Fold arithmetic cannot overflow.
        if(const double idx = ossia::convert<double>(*trig); std::isfinite(idx))
        {
          m_pos = std::llround(std::clamp(idx, -1e15, 1e15));
          m_started = true;
          emit = true;
        }
      }
      trig.reset();
    }

    switch(inputs.mode)
    {
      case Manual:
        break;
      case EveryTick:
        if(!emit)
        {
          step();
          emit = true;
        }
        break;
      case Timed: {
        // In frames: seconds accumulated in floating point drift against the
        // interval (0.1 s is not exact).
        const int64_t interval = std::max<int64_t>(
            1, std::llround(double(inputs.interval.value) * m_rate));
        m_elapsed += t.frames;
        if(m_elapsed >= interval)
        {
          m_elapsed %= interval;
          if(!emit)
          {
            step();
            emit = true;
          }
        }
        break;
      }
    }

    if(!emit || n == 0)
      return;

    const int idx = shape(m_pos, n, inputs.bounds);
    outputs.value.value = list[idx];
    if(idx != m_last_index)
    {
      outputs.index.value = idx;
      m_last_index = idx;
    }
  }

private:
  //! The first step goes to index 0.
  void step() noexcept
  {
    if(m_started)
      ++m_pos;
    m_started = true;
  }

  std::vector<ossia::value> m_list;
  double m_rate{48000.};
  int64_t m_elapsed{}; //!< frames since the last timed step
  int64_t m_pos{};
  int m_last_index{-1};
  bool m_started{};
};
}
