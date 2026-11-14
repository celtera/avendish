#pragma once

#include "Dsp.hpp"
#include "EQDiagram.hpp"
#include "ParametricEqControls.hpp"
#include <halp/audio.hpp>
#include <algorithm>
#include <functional>
#include <variant>

namespace examples
{
struct ParametricEq
{
  halp_meta(name, "Parametric Equalizer")
  halp_meta(c_name, "ngry_parameq")
  halp_meta(author, "Nicolas Gry; ossia score")
  halp_meta(category, "Audio/Effects/Filters")
  halp_meta(description, "Eight configurable serial filter bands with shared oversampling")
  halp_meta(uuid, "ec972d12-edba-4a9c-9736-9176efad17e3")

  struct ins
  {
    halp::dynamic_audio_bus<"Input", double> audio;
#define PARAMETRIQ_INPUTS(N) \
    parametriq::EnabledControl enabled##N; \
    parametriq::ChoiceControl<parametriq::ModelField, parametriq::RBJ> model##N; \
    parametriq::ChoiceControl<parametriq::ResponseField, parametriq::Bell> response##N; \
    parametriq::ChoiceControl<parametriq::Slope, 0> slope##N; \
    parametriq::ChoiceControl<parametriq::Character, 0> character##N; \
    parametriq::ChoiceControl<parametriq::Submodel, 0> submodel##N; \
    parametriq::ChoiceControl<parametriq::Order, 2> order##N; \
    parametriq::FrequencyControl frequency##N; \
    parametriq::QControl q##N; \
    parametriq::BandwidthControl bandwidth##N; \
    parametriq::GainControl gain##N; \
    parametriq::ShelfSlopeControl shelf_slope##N; \
    parametriq::RippleControl ripple##N; \
    parametriq::ResonanceControl resonance##N; \
    parametriq::DriveControl drive##N; \
    parametriq::TrimControl trim##N; \
    parametriq::ExtraControl extra##N; \
    parametriq::ChoiceControl<parametriq::Correction, 2> correction##N; \
    parametriq::SigmaControl sigma##N;
    PARAMETRIQ_INPUTS(1)
    PARAMETRIQ_INPUTS(2)
    PARAMETRIQ_INPUTS(3)
    PARAMETRIQ_INPUTS(4)
    PARAMETRIQ_INPUTS(5)
    PARAMETRIQ_INPUTS(6)
    PARAMETRIQ_INPUTS(7)
    PARAMETRIQ_INPUTS(8)
#undef PARAMETRIQ_INPUTS
    parametriq::QualityControl quality;
    parametriq::ReferenceControl reference_db;
  } inputs;
  struct
  {
    halp::mimic_audio_bus<"Output", &ins::audio> audio;
  } outputs;

  parametriq::Settings parameters() const
  {
#define PARAMETRIQ_VALUES(N) parametriq::parameters({ \
    inputs.enabled##N.value, inputs.model##N.value, inputs.response##N.value, \
    inputs.slope##N.value, inputs.character##N.value, inputs.submodel##N.value, \
    inputs.order##N.value, inputs.frequency##N.value, inputs.q##N.value, \
    inputs.bandwidth##N.value, inputs.gain##N.value, inputs.shelf_slope##N.value, \
    inputs.ripple##N.value, inputs.resonance##N.value, inputs.drive##N.value, \
    inputs.trim##N.value, inputs.extra##N.value, inputs.correction##N.value, inputs.sigma##N.value})
    return {{PARAMETRIQ_VALUES(1), PARAMETRIQ_VALUES(2), PARAMETRIQ_VALUES(3), PARAMETRIQ_VALUES(4),
             PARAMETRIQ_VALUES(5), PARAMETRIQ_VALUES(6), PARAMETRIQ_VALUES(7), PARAMETRIQ_VALUES(8)},
            inputs.quality.value, inputs.reference_db.value};
#undef PARAMETRIQ_VALUES
  }

  struct processor_to_ui { double rate = 48000.; };
  std::function<void(processor_to_ui)> send_message;
  parametriq::Engine engine;
  double rate = 48000.;
  int until_message = 0;
  bool storage_requested = false;

  // Off the audio thread: comb storage a band switched to comb waits for,
  // and the destruction of a state a new channel count replaced.
  using job = std::variant<int, parametriq::Engine>;
  struct
  {
    std::function<void(job)> request;
    static std::function<void(ParametricEq&)> work(job j)
    {
      const auto* lines = std::get_if<int>(&j);
      if(!lines)
        return {};
      auto storage = std::make_shared<parametriq::Engine::Storage>(
          parametriq::Engine::allocate(*lines));
      return [storage](ParametricEq& self) {
        self.engine.adopt(*storage);
        self.storage_requested = false;
      };
    }
  } worker;

  void prepare(halp::setup s)
  {
    rate = parametriq::sample_rate(s.rate);
    auto retired = engine.prepare(rate, s.input_channels, parameters());
    if(retired && worker.request)
      worker.request(std::move(retired));
    until_message = 0;
  }
  void operator()(int frames)
  {
    engine.update(parameters());
    engine.process(inputs.audio.samples, inputs.audio.channels,
        outputs.audio.samples, outputs.audio.channels, frames);
    if(const int missing = engine.missing_storage(); missing > 0 && !storage_requested && worker.request)
    {
      // Twice what is missing: a later transition of the band needs a copy.
      storage_requested = true;
      worker.request(2 * missing);
    }
    // Periodic metadata lets a newly opened view recover the current rate.
    if(send_message && frames > 0)
    {
      until_message -= frames;
      if(until_message <= 0)
      {
        send_message({rate});
        until_message = std::max(1, int(rate * 0.5));
      }
    }
  }

  template <int N, auto On, auto Model, auto Response, auto Slope, auto Character,
            auto Submodel, auto Order, auto Frequency, auto Q, auto Bandwidth,
            auto Gain, auto ShelfSlope, auto Ripple, auto Resonance, auto Drive,
            auto Trim, auto Extra, auto Correction, auto Sigma>
  struct BandPanel
  {
    halp_meta(name, N == 1 ? "Band 1" : N == 2 ? "Band 2" : N == 3 ? "Band 3" :
                   N == 4 ? "Band 4" : N == 5 ? "Band 5" : N == 6 ? "Band 6" :
                   N == 7 ? "Band 7" : "Band 8")
    halp_meta(layout, halp::layouts::vbox)
    template <parametriq::Field F, auto Control>
    using Item = parametriq::BandControl<F, Control, Model, Response, Slope,
        Character, Submodel, Order, Correction>;
    struct
    {
      halp_meta(layout, halp::layouts::hbox)
      halp::item<On> enabled;
      Item<parametriq::ModelField, Model> model;
      Item<parametriq::ResponseField, Response> response;
    } header;
    struct
    {
      halp_meta(layout, halp::layouts::hbox)
      Item<parametriq::Slope, Slope> slope;
      Item<parametriq::Character, Character> character;
      Item<parametriq::Submodel, Submodel> submodel;
      Item<parametriq::Order, Order> order;
    } configuration;
    struct
    {
      halp_meta(layout, halp::layouts::tabs)
      struct
      {
        halp_meta(name, "Shape")
        halp_meta(layout, halp::layouts::hbox)
        Item<parametriq::Frequency, Frequency> frequency;
        Item<parametriq::Q, Q> q;
        Item<parametriq::Bandwidth, Bandwidth> bandwidth;
        Item<parametriq::Gain, Gain> gain;
        Item<parametriq::ShelfSlope, ShelfSlope> shelf_slope;
        Item<parametriq::Resonance, Resonance> resonance;
        Item<parametriq::Extra, Extra> extra;
      } shape;
      struct
      {
        halp_meta(name, "Advanced")
        halp_meta(layout, halp::layouts::hbox)
        Item<parametriq::Drive, Drive> drive;
        Item<parametriq::Trim, Trim> trim;
        Item<parametriq::Ripple, Ripple> ripple;
        Item<parametriq::Correction, Correction> correction;
        Item<parametriq::Sigma, Sigma> sigma;
      } advanced;
    } parameters;
  };

#define PARAMETRIQ_MODELS(N) &ins::enabled##N, &ins::model##N, &ins::response##N, \
    &ins::slope##N, &ins::character##N, &ins::submodel##N, &ins::order##N, \
    &ins::frequency##N, &ins::q##N, &ins::bandwidth##N, &ins::gain##N, \
    &ins::shelf_slope##N, &ins::ripple##N, &ins::resonance##N, &ins::drive##N, \
    &ins::trim##N, &ins::extra##N, &ins::correction##N, &ins::sigma##N
  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp_meta(background, halp::colors::background_dark)
    halp::custom_multi_control<parametriq::EQDiagram,
        PARAMETRIQ_MODELS(1), PARAMETRIQ_MODELS(2), PARAMETRIQ_MODELS(3), PARAMETRIQ_MODELS(4),
        PARAMETRIQ_MODELS(5), PARAMETRIQ_MODELS(6), PARAMETRIQ_MODELS(7), PARAMETRIQ_MODELS(8),
        &ins::quality, &ins::reference_db> diagram;
    struct
    {
      halp_meta(layout, halp::layouts::hbox)
      halp::item<&ins::quality> quality;
      struct : halp::item<&ins::reference_db>
      {
        static constexpr bool dependent_choices() { return false; }
        static constexpr auto dependent_models()
        {
#define PARAMETRIQ_REFERENCE_MODELS(N) &ins::enabled##N, &ins::model##N, &ins::response##N, \
    &ins::slope##N, &ins::character##N, &ins::submodel##N, &ins::order##N, &ins::correction##N
          return std::tuple{PARAMETRIQ_REFERENCE_MODELS(1), PARAMETRIQ_REFERENCE_MODELS(2),
              PARAMETRIQ_REFERENCE_MODELS(3), PARAMETRIQ_REFERENCE_MODELS(4),
              PARAMETRIQ_REFERENCE_MODELS(5), PARAMETRIQ_REFERENCE_MODELS(6),
              PARAMETRIQ_REFERENCE_MODELS(7), PARAMETRIQ_REFERENCE_MODELS(8)};
#undef PARAMETRIQ_REFERENCE_MODELS
        }
        template <std::size_t Band, typename Values>
        static parametriq::Parameters band(const Values& v)
        {
          constexpr auto i = Band * 8;
          auto p = parametriq::configuration({std::get<i+1>(v), std::get<i+2>(v),
              std::get<i+3>(v), std::get<i+4>(v), std::get<i+5>(v), std::get<i+6>(v), std::get<i+7>(v)});
          p.enabled = std::get<i>(v);
          return p;
        }
        static bool visible(const auto& v)
        {
          parametriq::Settings s;
          [&]<std::size_t... I>(std::index_sequence<I...>) {
            ((s.bands[I] = band<I>(v)), ...);
          }(std::make_index_sequence<parametriq::band_count>{});
          return parametriq::response_kind(s) == parametriq::ResponseKind::Reference;
        }
        static std::string_view control_label(const auto&) { return "Reference (dBFS)"; }
      } reference;
    } global;
    struct
    {
      halp_meta(layout, halp::layouts::tabs)
      halp_meta(background, halp::colors::background_mid)
      static constexpr auto selection() { return "eq-band"; }
      BandPanel<1, PARAMETRIQ_MODELS(1)> band1;
      BandPanel<2, PARAMETRIQ_MODELS(2)> band2;
      BandPanel<3, PARAMETRIQ_MODELS(3)> band3;
      BandPanel<4, PARAMETRIQ_MODELS(4)> band4;
      BandPanel<5, PARAMETRIQ_MODELS(5)> band5;
      BandPanel<6, PARAMETRIQ_MODELS(6)> band6;
      BandPanel<7, PARAMETRIQ_MODELS(7)> band7;
      BandPanel<8, PARAMETRIQ_MODELS(8)> band8;
    } bands;
    struct bus
    {
      static void process_message(ui& self, processor_to_ui message)
      {
        const double rate = parametriq::sample_rate(message.rate);
        if(self.diagram.sample_rate != rate)
        {
          self.diagram.sample_rate = rate;
          if(self.diagram.update)
            self.diagram.update();
        }
      }
    };
  };
#undef PARAMETRIQ_MODELS
};
} // namespace examples
