#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Gamma/Delay.h>
#include <Gamma/ipl.h>
#include <halp/compat/gamma.hpp>
#include <halp/controls.hpp>
#include <halp/controls_fmt.hpp>
#include <halp/log.hpp>
#include <halp/meta.hpp>

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
  halp_meta(uuid, "39a7a489-a86b-4eaa-a617-ec2c9d559744")

  // Helper types for defining common cases of UI controls
  struct
  {
    halp::hslider_f32<"In"> in;
    struct : halp::hslider_i32<"Length">
    {
      void update(ValueDelay& self) { self.rebuild(); }
    } length;
    struct : halp::hslider_i32<"Count">
    {
      void update(ValueDelay& self) { self.rebuild(); }
    } count;
  } inputs;

  struct
  {
    halp::val_port<"Out", std::vector<float>> a;
  } outputs;

  void prepare(halp::setup info) noexcept
  {
    delay.set_sample_rate(500);
    delay.maxDelay(100.);
  }

  void rebuild()
  {
    delay.taps(inputs.count);
    for(int i = 0; i < inputs.count; i++)
      delay.delay(i * 0.11 + 0.1, i);
  }

  void operator()()
  {
    std::vector<float>& res = outputs.a.value;
    res.resize(inputs.count);
    float echo = 0.f;
    for(int i = 0; i < inputs.count; i++)
    {
      res[i] = delay.read(i);
      echo += res[i] * (1. / (1. + i));
    }

    delay(inputs.in.value + echo * 0.1);
  }

  gam::Multitap<float, gam::ipl::Linear, halp::compat::gamma_domain> delay{1, 1};
};

}
