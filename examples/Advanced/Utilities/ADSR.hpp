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
namespace detail
{
//! The envelopes of the ADSR objects: Trigger plays an AD, Hold gates an ADSR.
struct adsr_envelopes
{
  gam::AD<float, float, halp::compat::gamma_domain> ad;
  gam::ADSR<float, float, halp::compat::gamma_domain> adsr;
  bool was_holding = false;

  void prepare(double rate, bool holding) noexcept
  {
    ad.set_sample_rate(rate);
    adsr.set_sample_rate(rate);
    ad.finish();
    adsr.finish();
    was_holding = holding;
  }

  //! Runs the envelopes over a buffer; returns its loudest sample, so that
  //! envelopes shorter than one buffer are still visible in the single value
  //! the object outputs per buffer.
  float operator()(int frames, bool trig, bool hold) noexcept
  {
    if(trig)
      ad.reset();

    if(hold != was_holding)
    {
      was_holding = hold;
      if(hold)
        adsr.reset();
      else
        adsr.release();
    }

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
    return peak;
  }
};
}

/**
 * @brief Simple ADSR object implemented with Lance Putnam's Gamma library.
 */
struct ADSR
{
public:
  halp_meta(name, "ADSR (old)")
  halp_meta(c_name, "gamma_adsr")
  halp_meta(category, "Control/Mappings")
  halp_meta(description, "Trigger an ADSR envelope on input")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/adsr.html")
  halp_meta(author, "Lance Putnam, Gamma library")
  halp_flag(deprecated);
  halp_meta(uuid, "2d603ea6-be84-4f8c-8269-643e989f5997")

  struct
  {
    halp::maintained_button<"Hold"> hold;
    halp::impulse_button<"Trigger"> trig;

    struct : halp::knob_f32<"Attack", halp::range{0.0001, 100., 0.25}>
    {
      void update(ADSR& self) noexcept
      {
        self.env.ad.attack(value);
        self.env.adsr.attack(value);
      }
    } attack;

    struct : halp::knob_f32<"Decay", halp::range{0.0001, 100., 0.25}>
    {
      void update(ADSR& self) noexcept
      {
        self.env.ad.decay(value);
        self.env.adsr.decay(value);
      }
    } decay;

    struct : halp::knob_f32<"Sustain", halp::range{0., 1., 0.5}>
    {
      void update(ADSR& self) noexcept { self.env.adsr.sustain(value); }
    } sustain;

    struct : halp::knob_f32<"Release", halp::range{0.0001, 100., 0.25}>
    {
      void update(ADSR& self) noexcept { self.env.adsr.release(value); }
    } release;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Envelope")
      float value{};
    } out;
  } outputs;

  void prepare(halp::setup info) noexcept
  {
    env.prepare(info.rate, inputs.hold);
    outputs.out.value = 0.f;
  }

  void operator()(int frames) noexcept
  {
    outputs.out.value = env(frames, bool(inputs.trig), inputs.hold);
  }

private:
  detail::adsr_envelopes env;
};

/**
 * @brief ADSR with tempo-syncable stage times (time choosers).
 */
struct ADSR_v2
{
public:
  halp_meta(name, "ADSR")
  halp_meta(c_name, "gamma_adsr_v2")
  halp_meta(category, "Control/Mappings")
  halp_meta(description, "Trigger an ADSR envelope on input")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/adsr.html")
  halp_meta(author, "Lance Putnam, Gamma library")
  halp_meta(uuid, "108cfe30-8472-4699-a958-fa06eb30896a")

  struct
  {
    halp::maintained_button<"Hold"> hold;
    halp::impulse_button<"Trigger"> trig;

    struct : halp::time_chooser<"Attack", halp::range{0., 60., 0.01}>
    {
      void update(ADSR_v2& self) noexcept
      {
        self.env.ad.attack(stage_time(value));
        self.env.adsr.attack(stage_time(value));
      }
    } attack;

    struct : halp::time_chooser<"Decay", halp::range{0., 60., 0.25}>
    {
      void update(ADSR_v2& self) noexcept
      {
        self.env.ad.decay(stage_time(value));
        self.env.adsr.decay(stage_time(value));
      }
    } decay;

    struct : halp::knob_f32<"Sustain", halp::range{0., 1., 0.5}>
    {
      void update(ADSR_v2& self) noexcept { self.env.adsr.sustain(value); }
    } sustain;

    struct : halp::time_chooser<"Release", halp::range{0., 60., 0.25}>
    {
      void update(ADSR_v2& self) noexcept { self.env.adsr.release(stage_time(value)); }
    } release;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Envelope")
      float value{};
    } out;
  } outputs;

  //! Seconds, or a note value at the tempo (the time chooser converts): 0 is
  //! allowed and means as fast as the envelope goes.
  static float stage_time(float s) noexcept { return s > 1e-5f ? s : 1e-5f; }

  void prepare(halp::setup info) noexcept
  {
    env.prepare(info.rate, inputs.hold);
    outputs.out.value = 0.f;
  }

  void operator()(int frames) noexcept
  {
    outputs.out.value = env(frames, bool(inputs.trig), inputs.hold);
  }

private:
  detail::adsr_envelopes env;
};
}
