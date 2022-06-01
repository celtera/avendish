#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>

#include <vector>
#include <cmath>
#include <ratio>

namespace examples::helpers
{
consteval halp::range frequency_range() {
  return {20., 2000., 200.};
}
/**
 * Helpers version of the sine example
 */
struct Sine
{
  halp_meta(name, "Sines (helpers)")
  halp_meta(c_name, "avnd_helpers_sines")
  halp_meta(uuid, "0fa646c2-b292-4a03-9b7b-8c73f1dbd81b")

  struct inputs
  {
    struct : halp::knob_f32<"Log m=0.85", frequency_range()> {
      using mapper = halp::log_mapper<std::ratio<85, 100>>;
    } f1;
    struct : halp::hslider_f32<"Log m=0.15", frequency_range()> {
      using mapper = halp::log_mapper<std::ratio<15, 100>>;
    } f2;
    struct : halp::hslider_f32<"Inverse quintic", frequency_range()> {
      using mapper = halp::inverse_mapper<halp::pow_mapper<5>>;
    } f3;
    halp::hslider_f32<"Linear", frequency_range()> f4;
  };

  struct outputs
  {
    halp::audio_sample<"Out", float> audio;
  };

  float rate{};
  void prepare(halp::setup info)
  {
    this->rate = info.rate;
  }

  double phase[4] = { 0. };
  void operator()(const inputs& inputs, outputs& outputs)
  {
    const double phi[4] = {
      2 * 3.14 * inputs.f1.value / rate,
      2 * 3.14 * inputs.f2.value / rate,
      2 * 3.14 * inputs.f3.value / rate,
      2 * 3.14 * inputs.f4.value / rate,
    };
    outputs.audio.sample = 0.;
    for(int i = 0; i < 4; i++) {
      outputs.audio.sample += 0.15 * sin(phase[i] += phi[i]);
    }
  }
};
}
