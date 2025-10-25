#pragma once

#include <Gamma/Effects.h>
#include <halp/audio.hpp>
#include <halp/compat/gamma.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>

namespace ao
{
/**
 * @brief Simple bitcrush based on Lance Putnam's Gamma library
 */
struct Bitcrush
{
public:
  halp_meta(name, "Bitcrush")
  halp_meta(c_name, "gamma_bitcrush")
  halp_meta(author, "Lance Putnam, Gamma library")
  halp_meta(description, "Simple but efficient bitcrusher")
  halp_meta(category, "Audio/Effects")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-effects.html#bitcrush")
  halp_meta(uuid, "8ba36c05-f2c9-4cb2-b273-d726b56f9199")

  struct inputs
  {
    halp::hslider_f32<"Rate", halp::range{10., 22000., 4000.}> rate;
    halp::hslider_f32<"Crush", halp::range{0., 1., 0.5}> crush;
  };

  struct outputs
  {
  };

  void prepare(halp::setup info) noexcept { crush.set_sample_rate(info.rate); }

  double operator()(double v, const inputs& i, outputs& o) noexcept
  {
    crush.freq(i.rate);
    crush.step(i.crush);
    return crush(v);
  }

  gam::Quantizer<double, halp::compat::gamma_domain> crush;
};

}
