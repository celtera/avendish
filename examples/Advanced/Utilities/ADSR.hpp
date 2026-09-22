#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Gamma/Envelope.h>
#include <halp/audio.hpp>
#include <halp/compat/gamma.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <optional>

namespace ao
{
/**
 * @brief Simple ADSR object implemented with Lance Putnam's Gamma library.
 */
struct ADSR
{
public:
  halp_meta(name, "ADSR")
  halp_meta(c_name, "gamma_adsr")
  halp_meta(category, "Control/Mappings")
  halp_meta(description, "Trigger an ADSR envelope on input")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/adsr.html")
  halp_meta(author, "Lance Putnam, Gamma library")
  halp_meta(uuid, "2d603ea6-be84-4f8c-8269-643e989f5997")

  struct
  {
    halp::maintained_button<"Hold"> hold;
    halp::impulse_button<"Trigger"> trig;

    struct : halp::knob_f32<"Attack", halp::range{0.0001, 100., 0.25}>
    {
      void update(ADSR& self) noexcept
      {
        self.ad.attack(value);
        self.adsr.attack(value);
      }
    } attack;

    struct : halp::knob_f32<"Decay", halp::range{0.0001, 100., 0.25}>
    {
      void update(ADSR& self) noexcept
      {
        self.ad.decay(value);
        self.adsr.decay(value);
      }
    } decay;

    struct : halp::knob_f32<"Sustain", halp::range{0., 1., 0.5}>
    {
      void update(ADSR& self) noexcept { self.adsr.sustain(value); }
    } sustain;

    struct : halp::knob_f32<"Release", halp::range{0.0001, 100., 0.25}>
    {
      void update(ADSR& self) noexcept { self.adsr.release(value); }
    } release;
  } inputs;

  struct
  {
    struct
    {
      float value{};
    } out;
  } outputs;

  void prepare(halp::setup info) noexcept
  {
    ad.set_sample_rate(info.rate);
    adsr.set_sample_rate(info.rate);
    ad.finish();
    adsr.finish();
    was_holding = inputs.hold;
    outputs.out.value = 0.f;
  }

  void operator()(int frames) noexcept
  {
    if(inputs.trig)
    {
      ad.reset();
    }

    if(inputs.hold != was_holding)
    {
      was_holding = inputs.hold;
      if(was_holding)
      {
        adsr.reset();
      }
      else
      {
        adsr.release();
      }
    }

    // The envelopes run at audio rate but the output is a single value per
    // buffer: report the loudest sample of the buffer so that envelopes
    // shorter than one buffer are still visible.
    float peak = 0.f;
    for(int i = 0; i < frames; i++)
    {
      const bool ad_running = !ad.done();
      const bool adsr_running = !adsr.done();
      if(!ad_running && !adsr_running)
        break;

      const float a = ad_running ? ad() : 0.f;
      const float b = adsr_running ? adsr() : 0.f;
      const float v = a > b ? a : b;
      if(v > peak)
        peak = v;
    }

    outputs.out.value = peak;
  }

private:
  gam::AD<float, float, halp::compat::gamma_domain> ad;
  gam::ADSR<float, float, halp::compat::gamma_domain> adsr;
  bool was_holding = false;
};
}
