#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Gamma/Delay.h>
#include <Gamma/ipl.h>
#include <halp/audio.hpp>
#include <halp/compat/gamma.hpp>
#include <halp/controls.hpp>
#include <halp/controls_fmt.hpp>
#include <halp/log.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <deque>
#include <vector>

namespace examples::helpers
{
/**
 * Simple example of a value processor: takes float values as input, 
 * create a list with delayed values as output
 */
struct ValueDelay
{
  halp_meta(name, "Value delay")
  halp_meta(c_name, "avnd_value_delay")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Multitap delay for control input")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/value-delay.html")
  halp_meta(uuid, "39a7a489-a86b-4eaa-a617-ec2c9d559744")

  enum Mode
  {
    //! Tapped every tick: the delay depends on the tick rate (as it always did)
    Ticks,
    //! Tap i: the value (i+1) * Length changes of In ago
    Messages,
    //! Tap i: the value In had (i+1) * Time ago
    Time
  };

  // Helper types for defining common cases of UI controls
  struct
  {
    struct : halp::hslider_f32<"In">
    {
      void update(ValueDelay& self) { self.received(); }
    } in;
    struct : halp::hslider_i32<"Length">
    {
      void update(ValueDelay& self) { self.rebuild(); }
    } length;
    struct : halp::hslider_i32<"Count">
    {
      void update(ValueDelay& self) { self.rebuild(); }
    } count;
    halp::combobox_t<"Mode", Mode> mode;
    //! The spacing of the taps in Time mode. (Length is the spacing in the
    //! other modes: ticks or messages.)
    halp::time_chooser<"Time", halp::range{0.001, 60., 0.1}> time;
  } inputs;

  struct
  {
    halp::val_port<"Out", std::vector<float>> a;
  } outputs;


  void prepare(halp::setup info) noexcept
  {
    if(info.rate > 0)
      rate = info.rate;
    delay.set_sample_rate(500);
    delay.maxDelay(100.);
    rebuild();
  }

  void rebuild()
  {
    delay.taps(inputs.count);
    for(int i = 0; i < inputs.count; i++)
      delay.delay(i * 0.11 + 0.1, i);
  }

  void received()
  {
    const float v = inputs.in.value;
    messages.push_front(v);
    const std::size_t keep
        = std::size_t(std::max(1, inputs.length.value)) * std::max(1, inputs.count.value)
          + 1;
    while(messages.size() > keep)
      messages.pop_back();
    changes.emplace_back(now_ms, v);
  }

  //! In's value at time t (ms): the last change at or before it.
  float valueAt(double t) const noexcept
  {
    if(changes.empty())
      return inputs.in.value;
    auto it = std::upper_bound(
        changes.begin(), changes.end(), t,
        [](double t, const auto& c) { return t < c.first; });
    if(it == changes.begin())
      return it->second;
    return std::prev(it)->second;
  }

  // Without it, the bindings do not know operator() takes a tick and never
  // call it.
  using tick = halp::tick;
  void operator()(halp::tick tick)
  {
    const int count = std::max(0, inputs.count.value);
    const int length = std::max(1, inputs.length.value);
    std::vector<float>& res = outputs.a.value;
    res.resize(count);
    switch(inputs.mode)
    {
      default:
      case Ticks: {
        float echo = 0.f;
        for(int i = 0; i < count; i++)
        {
          res[i] = delay.read(i);
          echo += res[i] * (1. / (1. + i));
        }
        delay(inputs.in.value + echo * 0.1);
        break;
      }
      case Messages:
        for(int i = 0; i < count; i++)
        {
          const std::size_t back = std::size_t(i + 1) * length;
          res[i] = back < messages.size() ? messages[back]
                   : messages.empty()     ? inputs.in.value
                                          : messages.back();
        }
        break;
      case Time: {
        const double spacing_ms = 1000. * std::max(0.001f, inputs.time.value);
        for(int i = 0; i < count; i++)
          res[i] = valueAt(now_ms - double(i + 1) * spacing_ms);
        // Forget what no tap can reach anymore, keeping the value in force.
        const double horizon = now_ms - double(count) * spacing_ms;
        while(changes.size() > 1 && changes[1].first <= horizon)
          changes.pop_front();
        break;
      }
    }
    now_ms += 1000. * tick.frames / rate;
  }

  gam::Multitap<float, gam::ipl::Linear, halp::compat::gamma_domain> delay{1, 1};

private:
  double rate{500.};
  double now_ms{};
  // Messages mode: the values received, newest first.
  std::deque<float> messages;
  // Time mode: (time, value) at each change, oldest first.
  std::deque<std::pair<double, float>> changes;
};

}
