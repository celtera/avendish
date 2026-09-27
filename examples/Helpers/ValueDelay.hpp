#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

#include <algorithm>
#include <cmath>
#include <deque>
#include <optional>
#include <vector>

namespace examples::helpers
{
/**
 * A multitap delay for control values of any type: the list of what In was
 * one, two, ... delays ago, and one value mixing In with its first echo.
 *
 * The delay line holds the values In had, at positions counted in ticks, in
 * messages or in milliseconds depending on the mode. With Feedback, each echo
 * is mixed back into what the line records, so a movement repeats and fades
 * out, while a steady value stays what it is. Numbers, vectors and lists of
 * numbers mix; other values (strings...) are delayed as they are.
 */
struct ValueDelay
{
  halp_meta(name, "Value delay")
  halp_meta(c_name, "avnd_value_delay")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Multitap delay and echo for control values")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/value-delay.html")
  halp_meta(uuid, "39a7a489-a86b-4eaa-a617-ec2c9d559744")

  enum Mode
  {
    //! Tap i: the value (i+1) * Length ticks ago
    Ticks,
    //! Tap i: the value (i+1) * Length changes of In ago
    Messages,
    //! Tap i: the value In had (i+1) * Time ago
    Time
  };

  struct
  {
    //! Any value. Float-slider values of older documents load into it through
    //! the avnd process model's control -> value upgrade.
    halp::val_port<"In", std::optional<ossia::value>> in;
    halp::hslider_i32<"Length"> length;
    halp::hslider_i32<"Count"> count;
    halp::combobox_t<"Mode", Mode> mode;
    //! The spacing of the taps in Time mode, in seconds; a musical value
    //! follows the tempo (the binding converts it). Length is the spacing in
    //! the other modes: ticks or messages.
    halp::time_chooser<"Time", halp::range{0.001, 60., 0.1}> time;
    //! How much of each echo goes back into the line.
    halp::hslider_f32<"Feedback", halp::range{0., 0.99, 0.}> feedback;
    //! The Mix outlet: 0 is In, 1 its first echo.
    halp::hslider_f32<"Mix", halp::range{0., 1., 0.5}> mix;
    //! Stops recording In: the line repeats its last Length / Time.
    halp::toggle<"Freeze"> freeze;
    struct : halp::impulse_button<"Clear">
    {
      void update(ValueDelay& self) { self.clear(); }
    } clear;
    //! Time mode: glide between the recorded values rather than step.
    halp::toggle<"Smooth"> smooth;
  } inputs;

  struct
  {
    halp::val_port<"Out", std::vector<ossia::value>> a;
    halp::val_port<"Mix", ossia::value> mix;
  } outputs;

  static constexpr int max_taps = 1024;

  void prepare(halp::setup info) noexcept
  {
    if(info.rate > 0)
      rate = info.rate;
  }

  void clear() { line.clear(); }

  //! (1 - t) * a + t * b, for values that can be mixed: numbers, vectors
  //! and lists of such of the same size. Nothing for the rest.
  static std::optional<ossia::value>
  blend(const ossia::value& a, const ossia::value& b, float t)
  {
    auto number = [](const ossia::value& v) -> std::optional<float> {
      switch(v.get_type())
      {
        case ossia::val_type::FLOAT:
          return *v.target<float>();
        case ossia::val_type::INT:
          return float(*v.target<int>());
        case ossia::val_type::BOOL:
          return float(*v.target<bool>());
        default:
          return std::nullopt;
      }
    };
    if(auto x = number(a))
    {
      auto y = number(b);
      if(!y)
        return std::nullopt;
      const float r = (1.f - t) * *x + t * *y;
      if(a.get_type() == ossia::val_type::INT && b.get_type() == ossia::val_type::INT)
        return ossia::value{int(std::lround(r))};
      return ossia::value{r};
    }

    auto vec = [&]<std::size_t N>(const std::array<float, N>& x) -> std::optional<ossia::value> {
      auto* y = b.target<std::array<float, N>>();
      if(!y)
        return std::nullopt;
      std::array<float, N> r;
      for(std::size_t i = 0; i < N; i++)
        r[i] = (1.f - t) * x[i] + t * (*y)[i];
      return ossia::value{r};
    };
    if(auto* x = a.target<ossia::vec2f>())
      return vec(*x);
    if(auto* x = a.target<ossia::vec3f>())
      return vec(*x);
    if(auto* x = a.target<ossia::vec4f>())
      return vec(*x);

    if(auto* x = a.target<std::vector<ossia::value>>())
    {
      auto* y = b.target<std::vector<ossia::value>>();
      if(!y || y->size() != x->size())
        return std::nullopt;
      std::vector<ossia::value> r;
      r.reserve(x->size());
      for(std::size_t i = 0; i < x->size(); i++)
      {
        auto e = blend((*x)[i], (*y)[i], t);
        if(!e)
          return std::nullopt;
        r.push_back(std::move(*e));
      }
      return ossia::value{std::move(r)};
    }
    return std::nullopt;
  }

  //! What the line held at position p: the last value recorded at or before
  //! it, or glided to the next one when smoothing.
  ossia::value read(double p) const
  {
    if(line.empty())
      return last_in;
    auto it = std::upper_bound(
        line.begin(), line.end(), p, [](double p, const auto& e) { return p < e.pos; });
    if(it == line.begin())
      return it->value;
    auto prev = std::prev(it);
    if(smoothing() && it != line.end() && it->pos > prev->pos)
    {
      const float t = float((p - prev->pos) / (it->pos - prev->pos));
      if(auto v = blend(prev->value, it->value, t))
        return *v;
    }
    return prev->value;
  }

  // Without it, the bindings do not know operator() takes a tick and never
  // call it.
  using tick = halp::tick;
  void operator()(halp::tick tick)
  {
    const Mode mode = inputs.mode;
    if(mode != last_mode)
    {
      // Positions are ticks, messages or milliseconds: nothing carries over.
      line.clear();
      pos = 0.;
      last_mode = mode;
    }

    const bool received = inputs.in.value.has_value();
    if(received)
      last_in = *inputs.in.value;

    // A cable can send any count: the output list is allocated for it
    const int count = std::clamp(inputs.count.value, 0, max_taps);
    const double spacing
        = mode == Time ? 1000. * std::max(0.001f, inputs.time.value)
                       : double(std::max(1, inputs.length.value));

    switch(mode)
    {
      default:
      case Ticks:
        pos += 1.;
        record(last_in, spacing);
        break;
      case Messages:
        if(received && !inputs.freeze)
        {
          pos += 1.;
          record(last_in, spacing);
        }
        break;
      case Time:
        pos = now_ms;
        // With feedback or frozen, the line moves even while In does not.
        if(received || inputs.freeze || inputs.feedback > 0.f)
          record(last_in, spacing);
        break;
    }

    auto& res = outputs.a.value;
    res.resize(count);
    for(int i = 0; i < count; i++)
      res[i] = read(pos - double(i + 1) * spacing);

    const ossia::value wet = count > 0 ? res[0] : read(pos - spacing);
    const float mix = std::clamp(inputs.mix.value, 0.f, 1.f);
    if(auto m = blend(last_in, wet, mix))
      outputs.mix.value = std::move(*m);
    else
      outputs.mix.value = mix < 0.5f ? last_in : wet;

    // Forget what neither a tap nor the feedback can reach anymore, keeping
    // the value in force at the horizon.
    const double horizon = pos - double(std::max(count, 1) + 1) * spacing;
    while(line.size() > 1 && line[1].pos <= horizon)
      line.pop_front();

    if(rate > 0.)
      now_ms += 1000. * tick.frames / rate;
  }

private:
  struct entry
  {
    double pos{};
    ossia::value value;
  };

  bool smoothing() const noexcept { return inputs.smooth && inputs.mode == Time; }

  //! Records at `pos` what goes into the line: In, In mixed with its echo, or
  //! only the echo when frozen.
  void record(const ossia::value& in, double spacing)
  {
    ossia::value v;
    if(inputs.freeze)
    {
      if(line.empty())
        return;
      v = read(pos - spacing);
    }
    else if(const float fb = std::clamp(inputs.feedback.value, 0.f, 0.99f);
            fb > 0.f && !line.empty())
    {
      auto mixed = blend(in, read(pos - spacing), fb);
      v = mixed ? std::move(*mixed) : in;
    }
    else
    {
      v = in;
    }

    if(!line.empty() && line.back().pos >= pos)
      line.back().value = std::move(v);
    else if(line.empty() || !(line.back().value == v))
      line.push_back({pos, std::move(v)});
  }

  double rate{1000.};
  double now_ms{};
  //! Where the line is now: ticks, messages or milliseconds.
  double pos{};
  Mode last_mode{Ticks};
  //! The last value In received; what an empty line reads.
  ossia::value last_in{0.f};
  //! (position, value) at each change, oldest first.
  std::deque<entry> line;
};

}
