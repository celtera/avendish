#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "DynamicWaveshaper/Dsp.hpp"

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace ao
{
class DynamicWaveshaper
{
public:
  halp_meta(name, "Dynamic Waveshaper")
  halp_meta(c_name, "avnd_dynamic_waveshaper")
  halp_meta(category, "Audio/Distortion")
  halp_meta(author, "ossia score; Surge Synth Team")
  halp_meta(description, "Three-band waveshaping with dynamics and signed delayed feedback")
  halp_meta(uuid, "a4716e90-47a6-47bc-a8ec-9a41770018c5")

  // Serialization order is the pinned SST catalog with SURGE_XT_1X_WST=1.
  enum class Algorithm
  {
    Off,
    Soft,
    Hard,
    Asymmetric,
    Sine,
    Digital,
    SoftHarmonic2,
    SoftHarmonic3,
    SoftHarmonic4,
    SoftHarmonic5,
    FullWave,
    HalfWavePositive,
    HalfWaveNegative,
    SoftRectifier,
    SingleFold,
    DoubleFold,
    WestCoastFold,
    Additive12,
    Additive13,
    Additive14,
    Additive15,
    Additive12345,
    AdditiveSaw3,
    AdditiveSquare3,
    Fuzz,
    FuzzSoftClip,
    HeavyFuzz,
    FuzzCenter,
    FuzzSoftEdge,
    SinPlusX,
    Sin2xPlusX,
    Sin3xPlusX,
    Sin7xPlusX,
    Sin10xPlusX,
    Cycle2,
    Cycle7,
    Cycle10,
    Cycle2Bound,
    Cycle7Bound,
    Cycle10Bound,
    Medium,
    OJD,
    SoftSingleFold
  };
  static_assert(int(Algorithm::SoftSingleFold) + 1 == dynamic_waveshaper::algorithm_count);

  struct AlgorithmControl
  {
    halp_meta(name, "Algorithm")
    halp_meta(description, "Off bypasses shaping and Drive, not filtering or dynamics")
    enum widget
    {
      enumeration,
      list,
      combobox
    };
    struct range
    {
      std::array<std::string_view, 43> values{
          "Off", "Soft", "Hard", "Asymmetric", "Sine", "Digital",
          "Soft Harmonic 2", "Soft Harmonic 3", "Soft Harmonic 4", "Soft Harmonic 5",
          "Full Wave", "Half Wave Positive", "Half Wave Negative", "Soft Rectifier",
          "Single Fold", "Double Fold", "West Coast Fold",
          "Additive 1+2", "Additive 1+3", "Additive 1+4", "Additive 1+5",
          "Additive 12345", "Additive Saw 3", "Additive Square 3",
          "Fuzz", "Fuzz Soft Clip", "Heavy Fuzz", "Fuzz Center", "Fuzz Soft Edge",
          "Sin+x", "Sin 2x + x", "Sin 3x + x", "Sin 7x + x", "Sin 10x + x",
          "2 Cycle", "7 Cycle", "10 Cycle", "2 Cycle Bound", "7 Cycle Bound",
          "10 Cycle Bound", "Medium", "OJD", "Soft Single Fold"};
      Algorithm init = Algorithm::Off;
    };
    Algorithm value{Algorithm::Off};
  };

  static constexpr halp::look compact{.size = halp::control_size::compact};
  static constexpr halp::look frequency_knob{
      .hide_label = true, .widget = halp::control_widget::knob};
  static constexpr halp::look on_switch{
      .label = "On", .size = halp::control_size::compact};

  struct FilterControls
  {
    halp_flag(recursive_group);
    halp_meta(name, "Filters")
    halp::toggle<"High pass enabled"> highpass_enabled;
    halp::log_hslider_f32<"High pass", halp::range{20., 20000., 20.}> highpass_hz;
    halp::toggle<"Low pass enabled"> lowpass_enabled;
    halp::log_hslider_f32<"Low pass", halp::range{20., 20000., 20000.}> lowpass_hz;

    dynamic_waveshaper::FilterParameters parameters() const noexcept
    {
      return {.highpass_enabled = highpass_enabled.value,
              .lowpass_enabled = lowpass_enabled.value,
              .highpass_hz = highpass_hz.value,
              .lowpass_hz = lowpass_hz.value};
    }

    struct ui
    {
      halp_meta(layout, halp::layouts::hbox)
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&FilterControls::highpass_enabled, halp::label_as("High pass")> enabled;
        struct
        {
          halp_meta(layout, halp::layouts::container)
          static constexpr auto condition()
          {
            return halp::enabled_when<&FilterControls::highpass_enabled, true>{};
          }
          halp::item<&FilterControls::highpass_hz, frequency_knob> frequency;
        } frequency;
      } highpass;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&FilterControls::lowpass_enabled, halp::label_as("Low pass")> enabled;
        struct
        {
          halp_meta(layout, halp::layouts::container)
          static constexpr auto condition()
          {
            return halp::enabled_when<&FilterControls::lowpass_enabled, true>{};
          }
          halp::item<&FilterControls::lowpass_hz, frequency_knob> frequency;
        } frequency;
      } lowpass;
    };
  };

  template <int Band>
  struct BandFilters
  {
    halp_flag(recursive_group);
    halp_meta(name, Band == 0 ? "Band 1 filters" : Band == 1 ? "Band 2 filters" : "Band 3 filters")
    FilterControls pre;
    FilterControls post;

    void applyTo(dynamic_waveshaper::BandParameters& band) const noexcept
    {
      band.pre_filters = pre.parameters();
      band.post_filters = post.parameters();
    }

    struct pre_ui
    {
      halp_meta(name, "Pre")
      halp_meta(layout, halp::layouts::section)
      halp::recursive_group_item<&BandFilters::pre, FilterControls::ui> controls;
    };

    struct post_ui
    {
      halp_meta(name, "Post")
      halp_meta(layout, halp::layouts::section)
      halp::recursive_group_item<&BandFilters::post, FilterControls::ui> controls;
    };
  };

  template <int Band>
  struct BandControls
  {
    halp_flag(recursive_group);
    halp_meta(name, Band == 0 ? "Band 1" : Band == 1 ? "Band 2" : "Band 3")

    struct : halp::toggle<"Enabled", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Disabled bands contribute neither audio nor feedback sends")
    } enabled;
    halp::toggle<"Begin enabled", halp::toggle_setup{.init = Band != 0}> begin_enabled;
    halp::log_hslider_f32<"Begin", halp::range{20., 20000., Band == 2 ? 2500. : 250.}>
        begin_hz;
    halp::toggle<"End enabled", halp::toggle_setup{.init = Band != 2}> end_enabled;
    halp::log_hslider_f32<"End", halp::range{20., 20000., Band == 0 ? 250. : 2500.}>
        end_hz;
    AlgorithmControl algorithm;
    halp::knob_f32<"Drive", halp::range{-24., 36., 0.}> drive;
    struct : halp::knob_f32<"Level", halp::range{-60., 12., 0.}>
    {
      halp_meta(description, "Audible band level; feedback sends are taken before this level")
    } level;
    halp::toggle<"Dynamics", halp::toggle_setup{.init = false}> dynamics_enabled;
    halp::knob_f32<"Attack", halp::range{0.1, 200., 10.}> attack;
    halp::knob_f32<"Release", halp::range{5., 2000., 100.}> release;
    halp::knob_f32<"Threshold", halp::range{-60., 0., -18.}> threshold;
    halp::knob_f32<"Ratio", halp::range{1., 20., 4.}> ratio;

    dynamic_waveshaper::BandParameters parameters() const noexcept
    {
      return {
          .enabled = enabled.value,
          .begin_enabled = begin_enabled.value,
          .end_enabled = end_enabled.value,
          .dynamics_enabled = dynamics_enabled.value,
          .begin_hz = begin_hz.value,
          .end_hz = end_hz.value,
          .drive_db = drive.value,
          .level_db = level.value,
          .attack_ms = attack.value,
          .release_ms = release.value,
          .threshold_db = threshold.value,
          .ratio = ratio.value,
          .algorithm = int(algorithm.value),
          .pre_filters = {},
          .post_filters = {}};
    }

    template <std::size_t... I>
    static constexpr auto driveCondition(std::index_sequence<I...>)
    {
      return halp::enabled_when<&BandControls::algorithm, Algorithm(I + 1)...>{};
    }

    struct range_ui
    {
      halp_meta(layout, halp::layouts::vbox)
      halp::item<&BandControls::enabled, on_switch> enabled;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&BandControls::enabled, true>{};
        }
        struct
        {
          halp_meta(layout, halp::layouts::hbox)
          struct
          {
            halp_meta(layout, halp::layouts::vbox)
            halp::item<&BandControls::begin_enabled, halp::label_as("Begin")> enabled;
            struct
            {
              halp_meta(layout, halp::layouts::container)
              static constexpr auto condition()
              {
                return halp::enabled_when<&BandControls::begin_enabled, true>{};
              }
              halp::item<&BandControls::begin_hz, frequency_knob> frequency;
            } frequency;
          } begin;
          struct
          {
            halp_meta(layout, halp::layouts::vbox)
            halp::item<&BandControls::end_enabled, halp::label_as("End")> enabled;
            struct
            {
              halp_meta(layout, halp::layouts::container)
              static constexpr auto condition()
              {
                return halp::enabled_when<&BandControls::end_enabled, true>{};
              }
              halp::item<&BandControls::end_hz, frequency_knob> frequency;
            } frequency;
          } end;
        } range;
      } controls;
    };

    struct dynamics_ui
    {
      halp_meta(layout, halp::layouts::vbox)
      static constexpr auto condition()
      {
        return halp::enabled_when<&BandControls::enabled, true>{};
      }
      halp::item<&BandControls::dynamics_enabled, compact> dynamics_enabled;
      struct
      {
        halp_meta(layout, halp::layouts::grid)
        halp_meta(columns, 2)
        static constexpr auto condition()
        {
          return halp::enabled_when<&BandControls::dynamics_enabled, true>{};
        }
        halp::item<&BandControls::threshold, halp::label_as("Threshold")> threshold;
        halp::item<&BandControls::ratio, halp::label_as("Ratio")> ratio;
        halp::item<&BandControls::attack, halp::label_as("Attack")> attack;
        halp::item<&BandControls::release, halp::label_as("Release")> release;
      } dynamics;
    };

    struct shaping_ui
    {
      halp_meta(layout, halp::layouts::vbox)
      static constexpr auto condition()
      {
        return halp::enabled_when<&BandControls::enabled, true>{};
      }
      halp::item<&BandControls::algorithm,
                 halp::look{.hide_label = true,
                            .widget = halp::control_widget::combo}> algorithm;
      struct
      {
        halp_meta(layout, halp::layouts::container)
        static constexpr auto condition()
        {
          return driveCondition(std::make_index_sequence<42>{});
        }
        halp::item<&BandControls::drive, halp::label_as("Drive")> drive;
      } drive;
    };

    struct level_ui
    {
      halp_meta(layout, halp::layouts::container)
      static constexpr auto condition()
      {
        return halp::enabled_when<&BandControls::enabled, true>{};
      }
      halp::item<&BandControls::level, halp::label_as("Level")> level;
    };
  };

  struct ins
  {
    halp::audio_channel<"In", double> audio{};
    BandControls<0> band1;
    BandControls<1> band2;
    BandControls<2> band3;
    halp::knob_f32<"From 1 to 1", halp::range{-1., 1., 0.}> route11;
    halp::knob_f32<"From 2 to 1", halp::range{-1., 1., 0.}> route12;
    halp::knob_f32<"From 3 to 1", halp::range{-1., 1., 0.}> route13;
    halp::knob_f32<"From 1 to 2", halp::range{-1., 1., 0.}> route21;
    halp::knob_f32<"From 2 to 2", halp::range{-1., 1., 0.}> route22;
    halp::knob_f32<"From 3 to 2", halp::range{-1., 1., 0.}> route23;
    halp::knob_f32<"From 1 to 3", halp::range{-1., 1., 0.}> route31;
    halp::knob_f32<"From 2 to 3", halp::range{-1., 1., 0.}> route32;
    halp::knob_f32<"From 3 to 3", halp::range{-1., 1., 0.}> route33;
    struct : halp::toggle<"Feedback enabled", halp::toggle_setup{.init = false}>
    {
      halp_meta(description, "Signed, delayed pre-Level feedback; may self-oscillate")
    } feedback_enabled;
    halp::knob_f32<"Output", halp::range{-60., 12., 0.}> outputlevel;
    // Append controls to preserve the original preset and document port IDs.
    halp::knob_f32<"Input", halp::range{-24., 36., 0.}> inputlevel;
    FilterControls input_filters;
    BandFilters<0> band1_filters;
    BandFilters<1> band2_filters;
    BandFilters<2> band3_filters;
  } inputs;

  struct outs
  {
    halp::audio_channel<"Out", double> audio{};
    halp::hbargraph_f32<"Band 1 reduction", halp::range{0., 60., 0.}> gr1;
    halp::hbargraph_f32<"Band 2 reduction", halp::range{0., 60., 0.}> gr2;
    halp::hbargraph_f32<"Band 3 reduction", halp::range{0., 60., 0.}> gr3;
  } outputs;

  void prepare(halp::setup info) noexcept
  {
    engine.setParameters(parameters());
    engine.prepare(info.rate);
    prepared = true;
    outputs.gr1.value = outputs.gr2.value = outputs.gr3.value = 0.f;
  }

  void operator()(int frames) noexcept
  {
    if(frames <= 0)
      return;
    auto* const out = outputs.audio.channel;
    if(!out)
      return;
    if(!prepared)
    {
      std::fill_n(out, frames, 0.);
      outputs.gr1.value = outputs.gr2.value = outputs.gr3.value = 0.f;
      return;
    }

    engine.setParameters(parameters());

    const auto* const in = inputs.audio.channel;
    constexpr double limit = 64.;
    for(int i = 0; i < frames; ++i)
    {
      const double sample = in ? in[i] : 0.;
      const float input = dynamic_waveshaper::finite(sample)
                              ? float(std::clamp(sample, -limit, limit)) : 0.f;
      out[i] = engine.process(input);
    }
    outputs.gr1.value = engine.gainReduction(0);
    outputs.gr2.value = engine.gainReduction(1);
    outputs.gr3.value = engine.gainReduction(2);
  }

  template <auto Group, typename Controls, auto Filters, typename FilterGroup, auto Meter>
  struct BandPanel
  {
    halp_meta(name, Controls::name())
    halp_meta(layout, halp::layouts::section)
    halp_meta(background, halp::colors::background_mid)
    // Follow the DSP path without changing the serialized recursive groups.
    halp::recursive_group_item<Group, typename Controls::range_ui> range;
    halp::recursive_group_item<Filters, typename FilterGroup::pre_ui> pre;
    halp::recursive_group_item<Group, typename Controls::dynamics_ui> dynamics;
    halp::item<Meter, halp::label_as("Reduction")> reduction;
    halp::spacing separation{0, 6};
    halp::recursive_group_item<Group, typename Controls::shaping_ui> shaping;
    halp::recursive_group_item<Filters, typename FilterGroup::post_ui> post;
    halp::recursive_group_item<Group, typename Controls::level_ui> level;
  };

  struct ui
  {
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)
    BandPanel<&ins::band1, BandControls<0>, &ins::band1_filters, BandFilters<0>, &outs::gr1> band1;
    BandPanel<&ins::band2, BandControls<1>, &ins::band2_filters, BandFilters<1>, &outs::gr2> band2;
    BandPanel<&ins::band3, BandControls<2>, &ins::band3_filters, BandFilters<2>, &outs::gr3> band3;
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      struct
      {
        halp_meta(name, "Input")
        halp_meta(layout, halp::layouts::section)
        halp_meta(background, halp::colors::background_mid)
        halp::item<&ins::inputlevel, halp::label_as("Gain")> level;
        halp::recursive_group_item<&ins::input_filters, FilterControls::ui> filters;
      } input;
      struct
      {
        halp_meta(name, "Feedback")
        halp_meta(layout, halp::layouts::section)
        halp_meta(background, halp::colors::background_mid)
        halp::item<&ins::feedback_enabled, on_switch> enabled;
        struct
        {
          halp_meta(layout, halp::layouts::table)
          static constexpr auto columns()
          {
            return std::array<std::string_view, 3>{"From 1", "From 2", "From 3"};
          }
          static constexpr auto condition()
          {
            return halp::enabled_when<&ins::feedback_enabled, true>{};
          }
          struct
          {
            halp_meta(name, "To 1")
            halp_meta(layout, halp::layouts::hbox)
            halp::item<&ins::route11, compact> from1;
            halp::item<&ins::route12, compact> from2;
            halp::item<&ins::route13, compact> from3;
          } to1;
          struct
          {
            halp_meta(name, "To 2")
            halp_meta(layout, halp::layouts::hbox)
            halp::item<&ins::route21, compact> from1;
            halp::item<&ins::route22, compact> from2;
            halp::item<&ins::route23, compact> from3;
          } to2;
          struct
          {
            halp_meta(name, "To 3")
            halp_meta(layout, halp::layouts::hbox)
            halp::item<&ins::route31, compact> from1;
            halp::item<&ins::route32, compact> from2;
            halp::item<&ins::route33, compact> from3;
          } to3;
        } matrix;
      } feedback;
      struct
      {
        halp_meta(name, "Output")
        halp_meta(layout, halp::layouts::section)
        halp_meta(background, halp::colors::background_mid)
        halp_meta(alignment, halp::alignments::center)
        halp::item<&ins::outputlevel,
                   halp::look{.hide_label = true, .size = halp::control_size::large}> level;
      } output;
    } global;
  };

private:
  dynamic_waveshaper::Parameters parameters() const noexcept
  {
    dynamic_waveshaper::Parameters result;
    result.bands = {
        inputs.band1.parameters(), inputs.band2.parameters(), inputs.band3.parameters()};
    // Row-major M[destination][source]; diagonal entries are self-feedback.
    result.matrix = {
        inputs.route11.value, inputs.route12.value, inputs.route13.value,
        inputs.route21.value, inputs.route22.value, inputs.route23.value,
        inputs.route31.value, inputs.route32.value, inputs.route33.value};
    result.feedback_enabled = inputs.feedback_enabled.value;
    result.output_db = inputs.outputlevel.value;
    result.input_db = inputs.inputlevel.value;
    result.input_filters = inputs.input_filters.parameters();
    inputs.band1_filters.applyTo(result.bands[0]);
    inputs.band2_filters.applyTo(result.bands[1]);
    inputs.band3_filters.applyTo(result.bands[2]);
    return result;
  }

  dynamic_waveshaper::Engine engine;
  bool prepared{};
};
}
