#pragma once

#include <Gamma/Effects.h>
#include <Gamma/Oscillator.h>
#include <halp/audio.hpp>
#include <halp/compat/gamma.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>

#include <algorithm>

namespace ao
{
/**
 * @brief Simple flanger based on Lance Putnam's Gamma library
 */
struct Flanger
{
public:
  halp_meta(name, "Flanger (old)")
  halp_meta(c_name, "gamma_flanger")
  halp_meta(author, "Lance Putnam, Gamma library")
  halp_meta(category, "Audio/Effects")
  halp_meta(description, "Basic flanger audio effect")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-effects.html#flanger")
  halp_flag(deprecated);
  halp_meta(uuid, "538165be-18f0-4bbf-bcf0-ef9b97d6a1b1")

  struct inputs
  {
    halp::hslider_f32<"Amount", halp::range{0., 1e-2, 1e-3}> amount;
    struct : halp::time_chooser<"Delay", halp::range{0., 10., 0.5}>
    {
      using mapper = halp::log_mapper<std::ratio<95, 100>>;
    } delay;
    halp::hslider_f32<"Frequency", halp::range{0.001, 100., 0.5}> freq;
    halp::hslider_f32<"Feed-forward", halp::range{-0.99, 0.99, 0.7}> ffd;
    halp::hslider_f32<"Feed-back", halp::range{-0.99, 0.99, 0.7}> fbk;
  };

  struct outputs
  {
  };

  void prepare(halp::setup info) noexcept
  {
    comb.set_sample_rate(info.rate);
    mod.set_sample_rate(info.rate);
  }

  double operator()(double v, const inputs& i, outputs& o) noexcept
  {
    mod.freq(i.freq);
    comb.ffd(i.ffd);
    comb.fbk(i.fbk);
    comb.delay(i.delay + mod.cos() * i.amount);
    return comb(v);
  }

  gam::Comb<double, gam::ipl::AllPass, double, halp::compat::gamma_domain> comb{
      1. / 20., 1. / 500., 1, 0};
  gam::LFO<gam::phsInc::Loop, halp::compat::gamma_domain> mod{0.5};
};

/**
 * @brief Flanger whose sweep is a period, tempo-syncable.
 */
struct Flanger_v2
{
public:
  halp_meta(name, "Flanger")
  halp_meta(c_name, "gamma_flanger_v2")
  halp_meta(author, "Lance Putnam, Gamma library")
  halp_meta(category, "Audio/Effects")
  halp_meta(description, "Basic flanger audio effect")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-effects.html#flanger")
  halp_meta(uuid, "06163e09-9c14-4369-967c-ff869b4fcc0a")

  struct inputs
  {
    halp::hslider_f32<"Amount", halp::range{0., 1e-2, 1e-3}> amount;
    //! The centre of the sweep; a note value is clamped into the delay line.
    halp::time_chooser<"Delay", halp::range{0., 0.02, 0.002}> delay;
    //! One sweep of the delay: seconds, or a note value.
    halp::time_chooser<"Period", halp::range{0.01, 60., 2.}> period;
    halp::hslider_f32<"Feed-forward", halp::range{-0.99, 0.99, 0.7}> ffd;
    halp::hslider_f32<"Feed-back", halp::range{-0.99, 0.99, 0.7}> fbk;
  };

  struct outputs
  {
  };

  //! Length of the comb's delay line: longer than Delay + Amount at their
  //! maximum.
  static constexpr float max_delay = 1.f / 20.f;

  void prepare(halp::setup info) noexcept
  {
    comb.set_sample_rate(info.rate);
    mod.set_sample_rate(info.rate);

    // Gamma wraps a delay outside the line around it, and reads the oldest
    // sample for a delay of 0: the sweep, interpolating between two samples,
    // is kept two samples away from both ends.
    const float two_samples = 2.f / float(info.rate);
    m_min_delay = two_samples;
    m_max_delay = max_delay - two_samples;
  }

  double operator()(double v, const inputs& i, outputs& o) noexcept
  {
    mod.freq(1.f / std::max(1e-3f, i.period.value));
    comb.ffd(i.ffd);
    comb.fbk(i.fbk);
    comb.delay(std::clamp(i.delay + mod.cos() * i.amount, m_min_delay, m_max_delay));
    return comb(v);
  }

  gam::Comb<double, gam::ipl::AllPass, double, halp::compat::gamma_domain> comb{
      max_delay, 1. / 500., 1, 0};
  gam::LFO<gam::phsInc::Loop, halp::compat::gamma_domain> mod{0.5};
  float m_min_delay{};
  float m_max_delay{max_delay};
};

}
