#pragma once

#include "Model.hpp"
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <array>
#include <string_view>
#include <tuple>

namespace examples::parametriq
{
struct FrequencyControl : halp::knob_f32<"Frequency (Hz)", halp::range{20., 20000., 1000.}>
{
  using mapper = halp::log_mapper<std::ratio<85, 100>>;
};
// Octaves between the band's edges, as the RBJ cookbook's BW: 1/16 to 8
// octaves spans the bandwidth ranges of fil4 (1/16 to 4) and fomp's
// parametric (1/8 to 8), one octave their default.
struct BandwidthControl : halp::knob_f32<"Width (oct)", halp::range{0.0625, 8., 1.}>
{
  using mapper = halp::log_mapper<std::ratio<85, 100>>;
};
struct QControl : halp::knob_f32<"Q", halp::range{0.05, 50., 0.7071067811865476}>
{
  using mapper = halp::log_mapper<std::ratio<85, 100>>;
};
using EnabledControl = halp::toggle<"On", halp::toggle_setup{.init = false}>;
using GainControl = halp::knob_f32<"Gain (dB)", halp::range{-24., 24., 0.}>;
using ShelfSlopeControl = halp::knob_f32<"Shelf slope", halp::range{0.1, 1., 1.}>;
using RippleControl = halp::knob_f32<"Ripple (dB)", halp::range{0.001, 10., 1.}>;
using ResonanceControl = halp::knob_f32<"Resonance", halp::range{0., 1., 0.2}>;
using DriveControl = halp::knob_f32<"Drive (dB)", halp::range{-24., 36., 0.}>;
using TrimControl = halp::knob_f32<"Trim (dB)", halp::range{-60., 12., 0.}>;
using ExtraControl = halp::knob_f32<"Extra", halp::range{0., 1., 0.5}>;
using SigmaControl = halp::knob_f32<"Sigma", halp::range{0.01, 2., 0.636619772367581343}>;
using ReferenceControl = halp::knob_f32<"Reference (dBFS)", halp::range{-60., 0., -18.}>;

template <Field F, int Initial>
struct ChoiceControl
{
  static constexpr auto name()
  {
    if constexpr(F == ModelField) return "Model";
    else if constexpr(F == ResponseField) return "Response";
    else if constexpr(F == Slope) return "Slope / mix";
    else if constexpr(F == Character) return "Character";
    else if constexpr(F == Submodel) return "Submodel";
    else if constexpr(F == Order) return "Poles";
    else return "Correction";
  }
  enum widget { combobox };
  struct range
  {
    static constexpr auto names()
    {
      if constexpr(F == Order)
        return std::array<std::string_view, 15>{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14"};
      else if constexpr(F == Correction)
        return std::array<std::string_view, 3>{"None", "Omega", "Sigma"};
      else
        return all_choice_names<F>();
    }
    decltype(names()) values = names();
    int init = Initial;
  };
  int value = Initial;
};
struct QualityControl
{
  halp_meta(name, "Quality")
  enum widget { combobox };
  struct range
  {
    std::array<std::string_view, 3> values{"1x", "2x", "4x"};
    int init = 0;
  };
  int value = 0;
};

// Graph bindings and processor inputs use precisely the shared Field order.
using BandValues = std::tuple<bool, int, int, int, int, int, int,
    float, float, float, float, float, float, float, float, float, float, int, float>;
inline Parameters parameters(const BandValues& v)
{
  const auto& [on, model, response, slope, character, submodel, order,
      frequency, q, bandwidth, gain, shelf_slope, ripple, resonance, drive,
      trim, extra, correction, sigma] = v;
  return {on, model, response, slope, character, submodel, order,
      frequency, q, bandwidth, gain, shelf_slope, ripple, resonance, drive,
      trim, extra, correction, sigma};
}

// Native widgets need only categorical dependencies, not audio-rate values.
using ConfigurationValues = std::tuple<int, int, int, int, int, int, int>;
inline Parameters configuration(const ConfigurationValues& v)
{
  Parameters p;
  std::tie(p.model, p.response, p.slope, p.character, p.submodel, p.order, p.correction) = v;
  return resolve(p);
}
inline ConfigurationValues configuration_values(const Parameters& p)
{
  return {p.model, p.response, p.slope, p.character, p.submodel, p.order, p.correction};
}

template <Field F, auto Control, auto Model, auto Response, auto SlopeControl,
          auto CharacterControl, auto SubmodelControl, auto OrderControl, auto CorrectionControl>
struct BandControl : halp::item<Control>
{
  static constexpr auto dependent_models()
  {
    return std::tuple{Model, Response, SlopeControl, CharacterControl,
        SubmodelControl, OrderControl, CorrectionControl};
  }
  static constexpr bool dependent_choices()
  {
    return F == ModelField || F == ResponseField || F == Slope || F == Character
        || F == Submodel || F == Order || F == Correction;
  }
  static bool visible(const ConfigurationValues& v)
  {
    const auto p = configuration(v);
    if constexpr(dependent_choices() && F != ModelField)
      return applicable(p, F) && choices(p, F).count > 1;
    else
      return applicable(p, F);
  }
  static std::string_view control_label(const ConfigurationValues& v)
  {
    return label(configuration(v), F);
  }
  static Choices control_choices(const ConfigurationValues& v)
  {
    return choices(configuration(v), F);
  }
  static int selected(const ConfigurationValues& v)
  {
    const auto p = configuration(v);
    if constexpr(F == ModelField) return p.model;
    else if constexpr(F == ResponseField) return p.response;
    else if constexpr(F == Slope) return p.slope;
    else if constexpr(F == Character) return p.character;
    else if constexpr(F == Submodel) return p.submodel;
    else if constexpr(F == Order) return p.order;
    else return p.correction;
  }
  static ConfigurationValues edited(const ConfigurationValues& v, int value)
  {
    auto p = configuration(v);
    if constexpr(F == ModelField) p.model = value;
    else if constexpr(F == ResponseField) p.response = value;
    else if constexpr(F == Slope) p.slope = value;
    else if constexpr(F == Character) p.character = value;
    else if constexpr(F == Submodel) p.submodel = value;
    else if constexpr(F == Order) p.order = value;
    else if constexpr(F == Correction) p.correction = value;
    return configuration_values(resolve(p, F));
  }
};
} // namespace examples::parametriq
