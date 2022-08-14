#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Gamma/Envelope.h>
#include <halp/audio.hpp>
#include <halp/controls.hpp>

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
    gam::sampleRate(info.rate);
    ad.finish();
    adsr.finish();
    was_holding = false;
  }

  void operator()(int frames) noexcept
  {
    if(inputs.trig)
    {
      ad.reset();
    }

    if(inputs.hold != was_holding)
    {
      if(inputs.hold)
      {
        adsr.reset();
      }
      else
      {
        adsr.release();
      }
    }
    was_holding = inputs.hold;

    if(!adsr.done())
    {
      outputs.out.value = adsr();

      // We skip the next values in the buffer. Since the buffer
      // size are variables it's pretty much the only correct thing to do:
      // we "sample" an audio-level signal
      for(int i = 1; i < frames && !adsr.done(); i++)
        adsr();
    }

    // Trigger envelope overwrites held envelope
    if(!ad.done())
    {
      outputs.out.value = ad();
      for(int i = 1; i < frames && !ad.done(); i++)
        ad();
    }
  }

private:
  gam::AD<float, float> ad;
  gam::ADSR<float, float> adsr;
  bool was_holding = false;
};
}
