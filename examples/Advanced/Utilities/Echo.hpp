#pragma once

#include <Gamma/Effects.h>
#include <Gamma/Oscillator.h>
#include <halp/audio.hpp>
#include <halp/compat/gamma.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>

namespace ao
{
/**
 * @brief Simple flanger based on Lance Putnam's Gamma library
 */
struct Echo
{
public:
  halp_meta(name, "Echo")
  halp_meta(c_name, "gamma_echo")
  halp_meta(author, "Lance Putnam, Gamma library")
  halp_meta(category, "Audio/Effects")
  halp_meta(description, "Basic echo audio effect")
  halp_meta(uuid, "d7b7d998-e9d2-4575-9cf4-be5fec20195f")

  struct inputs
  {
    // Delay is in seconds
    struct : halp::time_chooser<"Delay", halp::range{0.001, 30., 0.2}>
    {
      using mapper = halp::log_mapper<std::ratio<95, 100>>;
    } delay;

    halp::hslider_f32<"Feedback", halp::range{0., 1, 0.2}> feedback;

    struct : halp::hslider_f32<"Filter", halp::range{0., 1, 0.5}>
    {
      using mapper = halp::log_mapper<std::ratio<65, 100>>;
    } filter;

    halp::hslider_f32<"Dry/Wet", halp::range{0., 1, 0.2}> drywet;
  };

  struct outputs
  {
  };

  void prepare(halp::setup info) noexcept
  {
    delay.set_sample_rate(info.rate);
    lpf.set_sample_rate(info.rate);

    lpf.type(gam::LOW_PASS);
    delay.maxDelay(30.);
  }

  double operator()(double s, const inputs& i, outputs& o) noexcept
  {
    // Update parameters
    delay.delay(i.delay);
    lpf.freq((1. - i.filter) * (3000. - 200.) + 200.);

    // Process
    auto echo = 10. * std::tanh((lpf(delay())) / 10.);
    auto fb_echo = echo * i.feedback;
    delay(s + fb_echo);

    return s * (1. - i.drywet) + echo * i.drywet;
  }

  gam::Delay<double, gam::ipl::Linear, halp::compat::gamma_domain> delay;
  gam::OnePole<double, double, halp::compat::gamma_domain> lpf;
};

}
