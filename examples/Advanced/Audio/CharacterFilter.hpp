#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "CharacterFilter/Dsp.hpp"
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <type_traits>

namespace ao
{
class CharacterFilter
{
public:
  halp_meta(name, "Character Filter")
  halp_meta(c_name, "avnd_character_filter")
  halp_meta(category, "Audio/Effects/Filters")
  halp_meta(author, "ossia score; Surge Synth Team")
  halp_meta(description, "Fifteen character filter families with drive, resonance and smoothed model changes")
  halp_meta(uuid, "901ee50d-e20d-4a72-aa0b-db34dc4d8402")

  template <int M>
  struct Variant
  {
    halp_meta(name, character_filter::models[M].name)
    enum widget { combobox };
    struct range
    {
      decltype(character_filter::variantNames<M>()) values = character_filter::variantNames<M>();
      int init = 0;
    };
    int value = 0;
  };
  struct ins
  {
    halp::dynamic_audio_bus<"Input", double> audio;
    halp::toggle<"Enabled", halp::toggle_setup{.init = true}> enabled;
    struct
    {
      halp_meta(name, "Model")
      enum widget { combobox };
      struct range
      {
        decltype(character_filter::modelNames()) values = character_filter::modelNames();
        int init = 0;
      };
      int value = 0;
    } model;
    halp::log_hslider_f32<"Cutoff (Hz)", halp::range{20., 20000., 1000.}> cutoff;
    struct : halp::knob_f32<"Resonance", halp::range{0., 1., 0.2}>
    {
      halp_meta(description, "Model-specific resonance, not a shared Q; some models self-oscillate")
    } resonance;
    halp::knob_f32<"Drive (dB)", halp::range{-24., 36., 0.}> drive;
    halp::knob_f32<"Output (dB)", halp::range{-60., 12., 0.}> output;
    // Each model remembers its last valid response/variant independently.
    Variant<0> v0;
    Variant<1> v1;
    Variant<2> v2;
    Variant<3> v3;
    Variant<4> v4;
    Variant<5> v5;
    Variant<6> v6;
    Variant<7> v7;
    Variant<8> v8;
    Variant<9> v9;
    Variant<10> v10;
    Variant<11> v11;
    Variant<12> v12;
    Variant<13> v13;
    Variant<14> v14;
    halp::knob_f32<"Pole morph", halp::range{0., 1., 0.5}> morph;
    halp::knob_f32<"Saturation", halp::range{0., 1., 0.5}> saturation;
    struct : halp::knob_f32<"Shelf / bell gain", halp::range{-1., 1., 0.}>
    {
      halp_meta(description, "SST's normalized bipolar gain; zero is unity")
    } gain;
    struct : halp::knob_f32<"Comb mix", halp::range{0., 1., 0.5}>
    {
      halp_meta(description, "Dry/wet; in bipolar mode: 0 = negative wet, 0.5 = dry, 1 = positive wet")
    } mix;
  } inputs;
  struct
  {
    halp::mimic_audio_bus<"Output", &ins::audio> audio;
  } outputs;

  character_filter::Parameters parameters() const noexcept
  {
    character_filter::Parameters p;
    p.model = inputs.model.value;
    switch(p.model)
    {
#define CF_VARIANT(N) case N: p.variant = inputs.v##N.value; break;
      CF_VARIANT(0) CF_VARIANT(1) CF_VARIANT(2) CF_VARIANT(3) CF_VARIANT(4)
      CF_VARIANT(5) CF_VARIANT(6) CF_VARIANT(7) CF_VARIANT(8) CF_VARIANT(9)
      CF_VARIANT(10) CF_VARIANT(11) CF_VARIANT(12) CF_VARIANT(13) CF_VARIANT(14)
#undef CF_VARIANT
    }
    switch(p.model)
    {
      case 3: p.extra = inputs.morph.value; break;
      case 5: p.extra = inputs.saturation.value; break;
      case 10: p.extra = (inputs.gain.value + 1.f) * 0.5f; break;
      case 12: p.extra = inputs.mix.value; break;
    }
    p.cutoff = inputs.cutoff.value;
    p.resonance = inputs.resonance.value;
    p.drive_db = inputs.drive.value;
    p.output_db = inputs.output.value;
    p.enabled = inputs.enabled.value;
    return p;
  }
  void prepare(halp::setup s)
  {
    engine.prepare(s.rate, s.input_channels, parameters());
  }
  void operator()(int frames) noexcept
  {
    engine.setParameters(parameters());
    engine.process(inputs.audio.samples, inputs.audio.channels,
                   outputs.audio.samples, outputs.audio.channels, frames);
  }

  struct EmptyPanel { halp_meta(layout, halp::layouts::container) };
  template <int M, auto V, auto E>
  struct ExtraPanel
  {
    halp_meta(layout, halp::layouts::container)
    struct Condition
    {
      static constexpr auto condition_control = V;
      static constexpr bool condition_hides = true;
      static constexpr auto condition_values() { return character_filter::extraVariants<M>(); }
    };
    static constexpr auto condition() { return Condition{}; }
    halp::item<E> control;
  };
  template <int M, auto V, auto E = nullptr>
  struct ModelPanel
  {
    halp_meta(name, character_filter::models[M].name.data())
    halp_meta(layout, halp::layouts::hbox)
    struct : halp::item<V, halp::label_as("Response / variant")>
    {
      static constexpr int width() { return 360; }
    } variant;
    std::conditional_t<(character_filter::extraVariants<M>().size() > 0),
                       ExtraPanel<M, V, E>, EmptyPanel> extra;
  };
  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp_meta(background, halp::colors::background_dark)
    struct
    {
      halp_meta(layout, halp::layouts::hbox)
      halp::item<&ins::enabled> enabled;
      halp::item<&ins::model> model;
    } header;
    struct ModelPages
    {
      halp_meta(layout, halp::layouts::tabs)
      halp_flag(hide_tabs);
      static constexpr auto model = &ins::model;
      ModelPanel<0, &ins::v0> classic;
      ModelPanel<1, &ins::v1> ladder;
      ModelPanel<2, &ins::v2> ob2;
      ModelPanel<3, &ins::v3, &ins::morph> ob4;
      ModelPanel<4, &ins::v4> xpander;
      ModelPanel<5, &ins::v5, &ins::saturation> k35;
      ModelPanel<6, &ins::v6> diode;
      ModelPanel<7, &ins::v7> vintage;
      ModelPanel<8, &ins::v8> cutoff_warp;
      ModelPanel<9, &ins::v9> resonance_warp;
      ModelPanel<10, &ins::v10, &ins::gain> svf;
      ModelPanel<11, &ins::v11> tripole;
      ModelPanel<12, &ins::v12, &ins::mix> comb;
      ModelPanel<13, &ins::v13> sample_hold;
      ModelPanel<14, &ins::v14> cascade;
    } models;
    struct
    {
      halp_meta(layout, halp::layouts::hbox)
      halp::item<&ins::cutoff, halp::look{.widget = halp::control_widget::knob}> cutoff;
      halp::item<&ins::resonance> resonance;
      halp::item<&ins::drive> drive;
      halp::item<&ins::output> output;
    } controls;
  };
private:
  character_filter::Engine engine;
};
}
