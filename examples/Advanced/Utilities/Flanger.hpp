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
struct Flanger
{
public:
  halp_meta(name, "Flanger")
  halp_meta(c_name, "gamma_flanger")
  halp_meta(author, "Lance Putnam, Gamma library")
  halp_meta(category, "Audio/Effects")
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

}
