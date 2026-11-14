#include "Model.hpp"
#include "../Audio/CharacterFilter/Catalog.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace examples::parametriq
{
namespace
{
namespace cf = ao::character_filter;
constexpr auto models = all_choice_names<ModelField>();
constexpr auto responses = all_choice_names<ResponseField>();
constexpr std::array response_ids{4, 8, 12, 16, 20, 24, 80, 96, 112,
                                 128, 146, 148, 48, 50, 53, 55, 0};
constexpr std::array slope_ids{0, 6, 18, 24, 36, 7, 8, 19, 25, 64,
                               80, 81, 82, 83, 84, 85, 86};
constexpr auto slopes = all_choice_names<Slope>();
constexpr std::array character_ids{0, 16, 18, 20, 24, 48, 50, 52, 54, 56, 57, 80, 64, 66, 68};
constexpr auto characters = all_choice_names<Character>();
constexpr std::array submodel_ids{0, 16, 32, 34, 36, 38, 39, 40, 49, 50, 51, 96, 97, 98, 99, 112};
constexpr auto submodels = all_choice_names<Submodel>();
constexpr std::array<std::string_view, 14> orders{
  "1 pole", "2 poles", "3 poles", "4 poles", "5 poles", "6 poles", "7 poles",
  "8 poles", "9 poles", "10 poles", "11 poles", "12 poles", "13 poles", "14 poles"};
constexpr std::array<std::string_view, 3> corrections{"None", "Omega", "Sigma"};
constexpr std::array<std::string_view, field_count> labels{
  "On", "Model", "Response", "Slope", "Character", "Implementation", "Order",
  "Frequency (Hz)", "Q", "Width (oct)", "Gain (dB)", "Shelf slope", "Ripple (dB)",
  "Resonance", "Drive (dB)", "Trim (dB)", "Extra", "Correction", "Sigma"};

bool band_response(int response) noexcept
{
  return response == BandPass || response == Notch || response == Bell;
}
bool gain_response(int response) noexcept
{
  return response == LowShelf || response == HighShelf || response == Bell;
}
bool classic(int model) noexcept { return model == Butterworth || model == ChebyshevI; }
bool character_model(int model) noexcept { return model >= VemberClassic && model < model_count; }
template <typename T> int index_of(const T& ids, int value) noexcept
{
  auto it = std::find(ids.begin(), ids.end(), value);
  return it == ids.end() ? -1 : int(it - ids.begin());
}
struct Tuple { int response, slope, character, submodel; };
Tuple tuple(const cf::Configuration& c) noexcept
{
  return {index_of(response_ids, c.passband), index_of(slope_ids, c.slope),
          index_of(character_ids, c.drive), index_of(submodel_ids, c.submodel)};
}
void add(Choices& result, int value, std::string_view name) noexcept
{
  if(value < 0 || result.count == int(result.entries.size()))
    return;
  for(const auto& c : result)
    if(c.value == value)
      return;
  result.entries[result.count++] = {value, name};
}
template <typename T> Choices named(const T& names, int offset = 0) noexcept
{
  Choices result;
  for(int i = 0; i < int(names.size()); ++i)
    add(result, i + offset, names[i]);
  return result;
}
double bounded(double value, double lo, double hi, double fallback) noexcept
{
  return std::clamp(std::isfinite(value) ? value : fallback, lo, hi);
}
int member(const Parameters& p, Field f) noexcept
{
  switch(f)
  {
    case ResponseField: return p.response;
    case Slope: return p.slope;
    case Character: return p.character;
    case Submodel: return p.submodel;
    default: return -1;
  }
}
int member(const Tuple& t, Field f) noexcept
{
  switch(f)
  {
    case ResponseField: return t.response;
    case Slope: return t.slope;
    case Character: return t.character;
    case Submodel: return t.submodel;
    default: return -1;
  }
}
}

std::span<const std::string_view, model_count> model_names() noexcept { return models; }
std::span<const std::string_view, response_count> response_names() noexcept { return responses; }
Choices all_choices(Field f) noexcept
{
  switch(f)
  {
    case ModelField: return named(models);
    case ResponseField: return named(responses);
    case Slope: return named(slopes);
    case Character: return named(characters);
    case Submodel: return named(submodels);
    case Order: return named(orders, 1);
    case Correction: return named(corrections);
    default: return {};
  }
}
Choices choices(const Parameters& p, Field field) noexcept
{
  if(field == ModelField)
    return all_choices(field);
  Choices result;
  if(character_model(p.model))
  {
    if(field < ResponseField || field > Submodel)
      return result;
    const auto& model = cf::models[p.model - VemberClassic];
    const auto names = all_choices(field);
    for(int i = model.begin; i < model.begin + model.count; ++i)
    {
      const auto t = tuple(cf::configurations[i]);
      if(field > ResponseField && t.response != p.response) continue;
      if(field > Slope && t.slope != p.slope) continue;
      if(field > Character && t.character != p.character) continue;
      const int value = member(t, field);
      add(result, value, names[value].name);
    }
    return result;
  }
  if(field == ResponseField)
  {
    for(int r = LowPass; r <= HighShelf; ++r)
    {
      if(r == Peak || (classic(p.model) && r == AllPass)) continue;
      add(result, r, responses[r]);
    }
  }
  else if(field == Order && (classic(p.model) || p.model == Ivantsov))
  {
    if(band_response(p.response))
      for(int n = 2; n <= 14; n += 2) add(result, n, orders[n - 1]);
    else
      for(int n = 1; n <= 7; ++n) add(result, n, orders[n - 1]);
  }
  else if(field == Correction && p.model == Ivantsov)
    return all_choices(field);
  return result;
}
Parameters resolve(Parameters p, Field changed) noexcept
{
  p.model = std::clamp(p.model, 0, int(model_count) - 1);
  if(character_model(p.model))
  {
    const auto& model = cf::models[p.model - VemberClassic];
    int best = model.begin, score = -1;
    for(int i = model.begin; i < model.begin + model.count; ++i)
    {
      const auto t = tuple(cf::configurations[i]);
      const int candidate = (t.response == p.response ? 8 : 0)
                          + (t.slope == p.slope ? 4 : 0)
                          + (t.character == p.character ? 2 : 0)
                          + (t.submodel == p.submodel ? 1 : 0)
                          + (changed >= ResponseField && changed <= Submodel
                             && member(t, changed) == member(p, changed) ? 32 : 0);
      if(candidate > score) { best = i; score = candidate; }
    }
    const auto t = tuple(cf::configurations[best]);
    p.response = t.response;
    p.slope = t.slope;
    p.character = t.character;
    p.submodel = t.submodel;
  }
  else
  {
    const auto responses = choices(p, ResponseField);
    bool found = false;
    for(const auto& c : responses) found |= c.value == p.response;
    if(!found) p.response = LowPass;
    p.slope = p.character = p.submodel = 0;
    const auto orders = choices(p, Order);
    if(orders.count)
    {
      int best = orders[0].value;
      for(const auto& c : orders)
        if(std::abs(c.value - std::clamp(p.order, 1, 14)) < std::abs(best - std::clamp(p.order, 1, 14)))
          best = c.value;
      p.order = best;
    }
    else p.order = 2;
  }
  p.correction = std::clamp(p.correction, 0, 2);
  return p;
}
int character_configuration(const Parameters& p) noexcept
{
  if(!character_model(p.model)) return -1;
  const auto& model = cf::models[p.model - VemberClassic];
  for(int i = model.begin; i < model.begin + model.count; ++i)
  {
    const auto t = tuple(cf::configurations[i]);
    if(t.response == p.response && t.slope == p.slope && t.character == p.character
       && t.submodel == p.submodel) return i;
  }
  return -1;
}
bool applicable(const Parameters& p, Field field) noexcept
{
  switch(field)
  {
    case Enabled: case ModelField: case Frequency: case Drive: case Trim: return true;
    case ResponseField: case Slope: case Character: case Submodel:
      return choices(p, field).count > 1;
    case Order: return classic(p.model) || p.model == Ivantsov;
    case Q:
      return (p.model == RBJ && p.response != LowShelf && p.response != HighShelf)
          || (p.model == Ivantsov && p.order > 1);
    case Bandwidth: return classic(p.model) && band_response(p.response);
    case Gain: return gain_response(p.response) && (p.model <= Ivantsov || p.model == FastSVF);
    case ShelfSlope: return p.model == RBJ && (p.response == LowShelf || p.response == HighShelf);
    case Ripple: return p.model == ChebyshevI;
    case Resonance: return character_model(p.model);
    case Extra:
    {
      const int c = character_configuration(p);
      return c >= 0 && cf::configurations[c].extra && p.model != FastSVF;
    }
    case Correction: return p.model == Ivantsov;
    case Sigma: return p.model == Ivantsov && p.correction == 0;
    default: return false;
  }
}
std::string_view label(const Parameters& p, Field f) noexcept
{
  if(f == Slope && p.model == Comb) return "Polarity / mix";
  if(f == Submodel && (p.model == CutoffWarp || p.model == ResonanceWarp)) return "Stages";
  if(f == Submodel && p.model == Tripole) return "Output";
  if(f == Q && p.model == Ivantsov && p.order > 2) return "Damping Q";
  if(f == Extra)
  {
    if(p.model == OBXd4) return "Pole morph";
    if(p.model == K35) return "Saturation";
    if(p.model == Comb) return "Comb mix";
  }
  return f >= Enabled && f < field_count ? labels[f] : std::string_view{};
}
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double min_bandwidth = 0.0625;
// The upper edge's highest angle: a thousandth of the rate below Nyquist.
constexpr double max_edge = 2. * pi * 0.499;
// The lower edge's angle w1 of the band whose upper edge is at w1 * ratio:
// tan(w1 / 2) tan(w1 ratio / 2) rises from 0 to infinity as w1 goes from 0 to
// pi / ratio, and passes tan^2(w0 / 2) once, below w0.
double lower_edge(double w0, double ratio) noexcept
{
  const double target = std::tan(w0 / 2.) * std::tan(w0 / 2.);
  double lo = 0., hi = std::min(w0, pi / ratio);
  for(int i = 0; i < 64; ++i)
  {
    const double w = 0.5 * (lo + hi);
    (std::tan(w / 2.) * std::tan(w * ratio / 2.) < target ? lo : hi) = w;
  }
  return 0.5 * (lo + hi);
}
}
BandEdges band_edges(double frequency, double octaves, double rate) noexcept
{
  const double ratio = std::exp2(octaves);
  const double w1 = lower_edge(2. * pi * frequency / rate, ratio);
  const double to_hz = rate / (2. * pi);
  return {w1 * to_hz, w1 * ratio * to_hz};
}
double max_bandwidth(double frequency, double rate) noexcept
{
  // With the upper edge at max_edge, tan(w1 / 2) = tan^2(w0 / 2) / tan(max_edge / 2).
  const double t0 = std::tan(pi * frequency / rate);
  const double w1 = 2. * std::atan(t0 * t0 / std::tan(max_edge / 2.));
  return std::log2(max_edge / w1);
}
double sample_rate(double rate) noexcept
{
  return std::isfinite(rate) && rate >= 1000. ? rate : 48000.;
}
Parameters sanitize(Parameters p, double rate) noexcept
{
  p = resolve(p);
  rate = sample_rate(rate);
  p.frequency = bounded(p.frequency, 20., std::min(20000., rate * 0.49), 1000.);
  p.q = bounded(p.q, 0.05, 50., 0.7071067811865476);
  p.bandwidth = bounded(p.bandwidth, min_bandwidth, std::min(8., max_bandwidth(p.frequency, rate)), 1.);
  p.gain = bounded(p.gain, -24., 24., 0.);
  p.shelf_slope = bounded(p.shelf_slope, 0.1, 1., 1.);
  p.ripple = bounded(p.ripple, 0.001, 10., 1.);
  if(classic(p.model) && gain_response(p.response))
    p.ripple = std::min(p.ripple, 0.99 * std::abs(p.gain));
  p.resonance = bounded(p.resonance, 0., 1., 0.2);
  p.drive = bounded(p.drive, -24., 36., 0.);
  p.trim = bounded(p.trim, -60., 12., 0.);
  p.extra = bounded(p.extra, 0., 1., 0.5);
  p.sigma = bounded(p.sigma, 0.01, 2., 0.636619772367581343);
  // Canonical effective controls make hidden edits inaudible and avoid needless fades.
  if(!applicable(p, Gain)) p.gain = 0.;
  if(!applicable(p, Q)) p.q = 0.7071067811865476;
  if(!applicable(p, Bandwidth)) p.bandwidth = 1.;
  if(!applicable(p, ShelfSlope)) p.shelf_slope = 1.;
  if(!applicable(p, Ripple)) p.ripple = 0.;
  if(!applicable(p, Resonance)) p.resonance = 0.2;
  if(!applicable(p, Extra)) p.extra = 0.5;
  if(!applicable(p, Correction)) p.correction = 2;
  if(!applicable(p, Sigma)) p.sigma = 0.636619772367581343;
  return p;
}
Settings sanitize(Settings s, double rate) noexcept
{
  for(auto& p : s.bands) p = sanitize(p, rate);
  s.quality = std::clamp(s.quality, 0, 2);
  s.reference_db = bounded(s.reference_db, -60., 0., -18.);
  return s;
}
ResponseKind response_kind(const Settings& s) noexcept
{
  auto kind = ResponseKind::Analytic;
  for(const auto& p : s.bands)
  {
    if(!p.enabled) continue;
    if(p.model == SampleHold) return ResponseKind::Unavailable;
    if(p.model >= VemberClassic && p.model != FastSVF) kind = ResponseKind::Reference;
  }
  return kind;
}
} // namespace examples::parametriq
