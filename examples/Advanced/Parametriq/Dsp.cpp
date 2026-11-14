/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "Dsp.hpp"
#include "Ivantsov/filters.h"
#include "../Audio/CharacterFilter/Catalog.hpp"
#include "../Audio/DynamicWaveshaper/Halfband.hpp"

#include <DspFilters/Butterworth.h>
#include <DspFilters/ChebyshevI.h>
#include <DspFilters/RBJ.h>
// sst/utilities/globals.h includes <math.h>, then SIMDe, which takes a
// defined HUGE_VAL to mean that <cmath> is there and calls std::isnan: the
// <math.h> of MSVC does not declare it.
#include <cmath>
#include <sst/filters++.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <new>
#include <numbers>
#include <type_traits>
#include <vector>

namespace examples::parametriq
{
namespace
{
namespace sf = sst::filtersplusplus;
namespace catalog = ao::character_filter;
namespace hb = ao::dynamic_waveshaper::halfband;
namespace iv = ivantsov::Linear;
using Complex = std::complex<double>;
using Frame = std::array<double, 4>;
constexpr double pi = std::numbers::pi;
constexpr int max_sections = 7;
// COMBquad writes a ring of MAX_FB_COMB samples, masked with MAX_FB_COMB - 1,
// plus FIRipol_N mirrored ones, whatever the delay time: a comb line is always
// this long, independently of the rate.
constexpr int delay_size = sst::filters::utilities::MAX_FB_COMB
                           + sst::filters::utilities::SincTable::FIRipol_N;
// Coefficients of the SST models are smoothed by their kernels over blocks of
// this many samples; a band stops refreshing them once SST's smoothing settled.
constexpr int sst_block = 16;
constexpr int sst_settle_blocks = 64;
// Ivantsov parameters are interpolated and set at most every this many samples.
constexpr int ivantsov_step = 8;
// The largest steps a running filter moves by: octaves of frequency, Q or
// relative width (the edges' distance over the frequency), dB of Ivantsov
// gain, and octaves of the Chebyshev ripple's epsilon.
constexpr double max_octaves = .5, max_db = 6., max_epsilon_octaves = 2.;

double amplitude(double db) noexcept { return std::pow(10., db / 20.); }
bool shelf(int response) noexcept
{
  return response == LowShelf || response == HighShelf || response == Bell;
}
bool band_transform(int response) noexcept
{
  return response == BandPass || response == Notch || response == Bell;
}
// A gain moved sample by sample, reaching its target exactly.
struct Ramp
{
  double value{1.}, target{1.}, step{};
  int left{};
  void set(double v) noexcept
  {
    value = target = v;
    left = 0;
  }
  void to(double v, int length) noexcept
  {
    target = v;
    if(length <= 1 || value == v)
    {
      value = v;
      left = 0;
      return;
    }
    step = (v - value) / length;
    left = length;
  }
  double next() noexcept
  {
    if(left)
      value = --left ? value + step : target;
    return value;
  }
};

// Defaults to the identity section.
struct Coefficients
{
  double b0{1.}, b1{}, b2{}, a1{}, a2{};
  Complex response(Complex z) const noexcept
  {
    return (b0 + z * (b1 + z * b2)) / (1. + z * (a1 + z * a2));
  }
};
struct Design
{
  std::array<Coefficients, max_sections> sections{};
  int count{};
  Complex response(double frequency, double rate) const noexcept
  {
    const auto z = std::polar(1., -2. * pi * frequency / rate);
    Complex result{1.};
    for(int i = 0; i < count; ++i)
      result *= sections[i].response(z);
    return result;
  }
};
// A section as a trapezoidal state-variable filter (Simper): its states are
// the integrators of a normalized analog prototype, which stay at signal level
// whatever the frequency, so the section can be retuned while it runs.
// H(s) = (m0 s^2 + (m0 k + m1) s + m0 + m2) / (s^2 + k s + 1), with
// s = (1 - z^-1) / (g (1 + z^-1)): any stable biquad, real poles included.
// Moving g, k and the taps linearly keeps g and k positive, so that every
// intermediate section is stable.
struct Section
{
  double g{1.}, k{2.}, m0{1.}, m1{}, m2{};
  double a1{.25}, a2{.25}, a3{.25};
  void update() noexcept
  {
    a1 = 1. / (1. + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
  }
  void add(const Section& d) noexcept
  {
    g += d.g; k += d.k; m0 += d.m0; m1 += d.m1; m2 += d.m2;
    update();
  }
  static Section step(const Section& from, const Section& to, int n) noexcept
  {
    return {(to.g - from.g) / n, (to.k - from.k) / n, (to.m0 - from.m0) / n,
            (to.m1 - from.m1) / n, (to.m2 - from.m2) / n};
  }
};
struct Sections
{
  std::array<Section, max_sections> sections{};
  int count{};
};
// The inverse bilinear transform of each biquad, normalized so that its
// denominator is s^2 + k s + 1. A section outside the stability triangle,
// which no design gives, passes the signal through.
Sections state_variable(const Design& d) noexcept
{
  Sections result;
  result.count = d.count;
  for(int i = 0; i < d.count; ++i)
  {
    const auto& c = d.sections[i];
    const double plus = 1. + c.a1 + c.a2, minus = 1. - c.a1 + c.a2;
    Section s;
    s.g = std::sqrt(plus / minus);
    s.k = 2. * (1. - c.a2) / (s.g * minus);
    s.m0 = (c.b0 - c.b1 + c.b2) / minus;
    s.m1 = 2. * (c.b0 - c.b2) / (s.g * minus) - s.m0 * s.k;
    s.m2 = (c.b0 + c.b1 + c.b2) / plus - s.m0;
    s.update();
    const bool valid = plus > 0. && minus > 0. && s.k > 0. && std::isfinite(s.g)
                       && std::isfinite(s.m0) && std::isfinite(s.m1) && std::isfinite(s.m2);
    result.sections[i] = valid ? s : Section{};
  }
  return result;
}
Coefficients coefficients(const Dsp::BiquadBase& c) noexcept
{
  const double a0 = c.getA0();
  return {c.getB0() / a0, c.getB1() / a0, c.getB2() / a0,
          c.getA1() / a0, c.getA2() / a0};
}
template <typename Filter, typename... Args>
Design classic_filter(Args... args) noexcept
{
  Filter filter;
  filter.setup(args...);
  Design result;
  result.count = filter.getNumStages();
  for(int i = 0; i < result.count; ++i)
    result.sections[i] = coefficients(filter[i]);
  return result;
}
template <typename Filter, typename... Args>
Design biquad(Args... args) noexcept
{
  Filter filter;
  filter.setup(args...);
  Design result;
  result.count = 1;
  result.sections[0] = coefficients(filter);
  return result;
}
// Where a classic design's reference gain lies, away from every section's
// zeros: the band-pass centre, Nyquist above a high-pass or low shelf, DC
// otherwise.
double reference_angle(const Parameters& p, double rate) noexcept
{
  switch(p.response)
  {
    case HighPass:
    case LowShelf: return pi;
    case BandPass: return 2. * pi * p.frequency / rate;
    default: return 0.;
  }
}
// DSPFilters scales the first section to the design's gain and leaves the
// others with gains up to (rate / frequency)^2 at the reference: the signal
// between sections would span orders of magnitude, and a state built under one
// setting would meet the taps of another as the filter moves. Every section
// takes the same share of the gain at the reference angle instead; the
// response is unchanged, and the state-variable sections keep their poles, so
// only their output taps scale.
Design balanced(Design d, double angle) noexcept
{
  const auto z = std::polar(1., -angle);
  std::array<double, max_sections> gains{};
  double total = 0.;
  for(int i = 0; i < d.count; ++i)
  {
    gains[i] = std::abs(d.sections[i].response(z));
    if(!(gains[i] > 0.) || !std::isfinite(gains[i]))
      return d;
    total += std::log(gains[i]);
  }
  const double share = std::exp(total / d.count);
  for(int i = 0; i < d.count; ++i)
  {
    auto& c = d.sections[i];
    const double scale = share / gains[i];
    c.b0 *= scale;
    c.b1 *= scale;
    c.b2 *= scale;
  }
  return d;
}
// DSPFilters' band transforms take the edges' midpoint and distance in Hz and
// put the prototype's centre (the bell's peak, band-pass maximum, notch null)
// at w0 with cos w0 = cos(wm) / cos(wd / 2), that is tan^2(w0 / 2) =
// tan(w1 / 2) tan(w2 / 2): band_edges places the edges `bandwidth` octaves
// apart on that relation. The prototypes are at their edge level there: -3 dB
// for the Butterworth band-pass and notch, half the gain for its bell, and
// the ripple below the band's level for Chebyshev I (Orfanidis 2005).
struct BandArguments { double centre, width; };
BandArguments band_arguments(const Parameters& p, double rate) noexcept
{
  const auto [low, high] = band_edges(p.frequency, p.bandwidth, rate);
  return {0.5 * (low + high), high - low};
}
// The classic designs as DSPFilters gives them.
Design classic_design(const Parameters& p, double rate, int order) noexcept
{
  const auto [centre, width] = band_transform(p.response) ? band_arguments(p, rate) : BandArguments{};
  if(p.model == Butterworth)
  {
    switch(p.response)
    {
      case LowPass: return classic_filter<Dsp::Butterworth::LowPass<7>>(order, rate, p.frequency);
      case HighPass: return classic_filter<Dsp::Butterworth::HighPass<7>>(order, rate, p.frequency);
      case BandPass: return classic_filter<Dsp::Butterworth::BandPass<7>>(order, rate, centre, width);
      case Notch: return classic_filter<Dsp::Butterworth::BandStop<7>>(order, rate, centre, width);
      case LowShelf: return classic_filter<Dsp::Butterworth::LowShelf<7>>(order, rate, p.frequency, p.gain);
      case HighShelf: return classic_filter<Dsp::Butterworth::HighShelf<7>>(order, rate, p.frequency, p.gain);
      case Bell: return classic_filter<Dsp::Butterworth::BandShelf<7>>(order, rate, centre, width, p.gain);
      default: return {};
    }
  }
  if(p.model == ChebyshevI)
  {
    switch(p.response)
    {
      case LowPass: return classic_filter<Dsp::ChebyshevI::LowPass<7>>(order, rate, p.frequency, p.ripple);
      case HighPass: return classic_filter<Dsp::ChebyshevI::HighPass<7>>(order, rate, p.frequency, p.ripple);
      case BandPass: return classic_filter<Dsp::ChebyshevI::BandPass<7>>(order, rate, centre, width, p.ripple);
      case Notch: return classic_filter<Dsp::ChebyshevI::BandStop<7>>(order, rate, centre, width, p.ripple);
      case LowShelf: return classic_filter<Dsp::ChebyshevI::LowShelf<7>>(order, rate, p.frequency, p.gain, p.ripple);
      case HighShelf: return classic_filter<Dsp::ChebyshevI::HighShelf<7>>(order, rate, p.frequency, p.gain, p.ripple);
      case Bell: return classic_filter<Dsp::ChebyshevI::BandShelf<7>>(order, rate, centre, width, p.gain, p.ripple);
      default: return {};
    }
  }
  return {};
}
// A design always has the same number of sections for a model, response and
// order, so that its coefficients can move from one setting to the next.
// Invalid tuples (none come from sanitize) pass the signal through.
Design linear_design(const Parameters& p, double rate) noexcept
{
  const int order = band_transform(p.response) ? p.order / 2 : p.order;
  if(shelf(p.response) && p.gain == 0.)
  {
    Design identity;
    identity.count = p.model == RBJ ? 1
        : std::clamp(band_transform(p.response) ? order : (order + 1) / 2, 0, max_sections);
    return identity;
  }
  if(p.model == RBJ)
  {
    switch(p.response)
    {
      case LowPass: return biquad<Dsp::RBJ::LowPass>(rate, p.frequency, p.q);
      case HighPass: return biquad<Dsp::RBJ::HighPass>(rate, p.frequency, p.q);
      // Despite the library argument's name, these two take Q, not bandwidth.
      case BandPass: return biquad<Dsp::RBJ::BandPass2>(rate, p.frequency, p.q);
      case Notch: return biquad<Dsp::RBJ::BandStop>(rate, p.frequency, p.q);
      case AllPass: return biquad<Dsp::RBJ::AllPass>(rate, p.frequency, p.q);
      case LowShelf: return biquad<Dsp::RBJ::LowShelf>(rate, p.frequency, p.gain, p.shelf_slope);
      case HighShelf: return biquad<Dsp::RBJ::HighShelf>(rate, p.frequency, p.gain, p.shelf_slope);
      case Bell:
      {
        // RBJ's bell takes digital octave bandwidth. Invert its alpha formula
        // so the exposed Q retains the cookbook's precise alpha = sin(w)/(2Q).
        const double w = 2. * pi * p.frequency / rate;
        const double bw = 2. / std::log(2.) * std::asinh(1. / (2. * p.q)) * std::sin(w) / w;
        return biquad<Dsp::RBJ::BandShelf>(rate, p.frequency, p.gain, bw);
      }
      default: return {};
    }
  }
  if(order < 1 || order > max_sections)
    return {};
  return balanced(classic_design(p, rate, order), reference_angle(p, rate));
}

// All Ivantsov instantiations are trivially destructible, bounded-size objects.
// Dispatch is resolved once on selection, with no variant visit per sample.
// A setting the library refuses passes the signal through.
struct IvantsovFilter
{
  using Largest = iv::Filter<iv::BandShelf, 14, double>;
  alignas(Largest) std::array<std::byte, sizeof(Largest)> storage{};
  using LargestResponse = Largest::Response;
  alignas(LargestResponse) std::array<std::byte, sizeof(LargestResponse)> snapshot{};
  void (*capture_fn)(const void*, void*) noexcept{};
  double (*process_fn)(void*, double) noexcept{};
  Complex (*response_fn)(const void*, double) noexcept{};
  void (*reset_fn)(void*) noexcept{};
  bool (*set_fn)(void*, const Parameters&) noexcept{};

  IvantsovFilter() noexcept { bypass(); }
  void bypass() noexcept
  {
    capture_fn = [](const void*, void*) noexcept { };
    process_fn = [](void*, double x) noexcept { return x; };
    response_fn = [](const void*, double) noexcept { return Complex{1.}; };
    reset_fn = [](void*) noexcept { };
    set_fn = [](void*, const Parameters&) noexcept { return false; };
  }
  // One transaction: intermediate settings need not be representable. A
  // valid update keeps the filter's state, an invalid one changes nothing.
  template <typename Filter, iv::Type Type, int Order, ivantsov::Correction Correction>
  static bool apply(Filter& f, const Parameters& p) noexcept
  {
    if constexpr(Correction == ivantsov::Correction::None)
    {
      if constexpr(iv::is_shelf<Type> && Order > 1)
        return f.template set<iv::Fc, iv::Sigma, iv::Gain, iv::Zeta>(p.frequency, p.sigma, amplitude(p.gain), 1. / (2. * p.q));
      else if constexpr(iv::is_shelf<Type>)
        return f.template set<iv::Fc, iv::Sigma, iv::Gain>(p.frequency, p.sigma, amplitude(p.gain));
      else if constexpr(Order > 1)
        return f.template set<iv::Fc, iv::Sigma, iv::Zeta>(p.frequency, p.sigma, 1. / (2. * p.q));
      else
        return f.template set<iv::Fc, iv::Sigma>(p.frequency, p.sigma);
    }
    else
    {
      if constexpr(iv::is_shelf<Type> && Order > 1)
        return f.template set<iv::Fc, iv::Gain, iv::Zeta>(p.frequency, amplitude(p.gain), 1. / (2. * p.q));
      else if constexpr(iv::is_shelf<Type>)
        return f.template set<iv::Fc, iv::Gain>(p.frequency, amplitude(p.gain));
      else if constexpr(Order > 1)
        return f.template set<iv::Fc, iv::Zeta>(p.frequency, 1. / (2. * p.q));
      else
        return f.template set<iv::Fc>(p.frequency);
    }
  }
  template <iv::Type Type, int Order, ivantsov::Correction Correction>
  void select(const Parameters& p, double rate) noexcept
  {
    using Filter = iv::Filter<Type, Order, double, Correction>;
    static_assert(sizeof(Filter) <= sizeof(Largest));
    static_assert(alignof(Filter) <= alignof(Largest));
    static_assert(std::is_trivially_destructible_v<Filter>);
    auto* f = ::new(storage.data()) Filter{};
    if(!f->initialise(rate) || !apply<Filter, Type, Order, Correction>(*f, p))
    {
      bypass();
      return;
    }
    process_fn = [](void* data, double x) noexcept { return static_cast<Filter*>(data)->processed(x); };
    using Response = typename Filter::Response;
    static_assert(sizeof(Response) <= sizeof(LargestResponse));
    capture_fn = [](const void* data, void* destination) noexcept {
      ::new(destination) Response{static_cast<const Filter*>(data)->response()};
    };
    response_fn = [](const void* data, double hz) noexcept {
      return static_cast<const Response*>(data)->complex_response_hz(hz);
    };
    reset_fn = [](void* data) noexcept { static_cast<Filter*>(data)->reset(); };
    set_fn = [](void* data, const Parameters& p) noexcept {
      return apply<Filter, Type, Order, Correction>(*static_cast<Filter*>(data), p);
    };
  }
  template <iv::Type Type, int Order>
  void correction(const Parameters& p, double rate) noexcept
  {
    switch(p.correction)
    {
      case 0: select<Type, Order, ivantsov::Correction::None>(p, rate); break;
      case 1: select<Type, Order, ivantsov::Correction::Omega>(p, rate); break;
      case 2: select<Type, Order, ivantsov::Correction::Sigma>(p, rate); break;
      default: bypass();
    }
  }
  template <iv::Type Type>
  void order(const Parameters& p, double rate) noexcept
  {
    if constexpr(iv::is_band<Type>)
    {
      switch(p.order)
      {
        case 2: correction<Type, 2>(p, rate); break;
        case 4: correction<Type, 4>(p, rate); break;
        case 6: correction<Type, 6>(p, rate); break;
        case 8: correction<Type, 8>(p, rate); break;
        case 10: correction<Type, 10>(p, rate); break;
        case 12: correction<Type, 12>(p, rate); break;
        case 14: correction<Type, 14>(p, rate); break;
        default: bypass();
      }
    }
    else
    {
      switch(p.order)
      {
        case 1: correction<Type, 1>(p, rate); break;
        case 2: correction<Type, 2>(p, rate); break;
        case 3: correction<Type, 3>(p, rate); break;
        case 4: correction<Type, 4>(p, rate); break;
        case 5: correction<Type, 5>(p, rate); break;
        case 6: correction<Type, 6>(p, rate); break;
        case 7: correction<Type, 7>(p, rate); break;
        default: bypass();
      }
    }
  }
  void configure(const Parameters& p, double rate) noexcept
  {
    switch(p.response)
    {
      case LowPass: order<iv::LowPass>(p, rate); break;
      case HighPass: order<iv::HighPass>(p, rate); break;
      case BandPass: order<iv::BandPass>(p, rate); break;
      case Notch: order<iv::BandStop>(p, rate); break;
      case AllPass: order<iv::AllPass>(p, rate); break;
      case LowShelf: order<iv::LowShelf>(p, rate); break;
      case HighShelf: order<iv::HighShelf>(p, rate); break;
      case Bell: order<iv::BandShelf>(p, rate); break;
      default: bypass();
    }
  }
  bool set(const Parameters& p) noexcept { return set_fn(storage.data(), p); }
  double process(double x) noexcept
  {
    const double y = process_fn(storage.data(), x);
    if(std::isfinite(y))
      return y;
    reset_fn(storage.data());
    return 0.;
  }
  void capture_response() noexcept { capture_fn(storage.data(), snapshot.data()); }
  Complex response(double frequency) const noexcept
  {
    return response_fn(snapshot.data(), frequency);
  }
};
// The Ivantsov controls a fraction t of the way from a to b: frequency and Q
// geometrically, gain in dB and sigma linearly.
Parameters interpolate(const Parameters& a, const Parameters& b, double t) noexcept
{
  if(t >= 1.)
    return b;
  auto p = b;
  p.frequency = a.frequency * std::pow(b.frequency / a.frequency, t);
  p.q = a.q * std::pow(b.q / a.q, t);
  p.gain = a.gain + t * (b.gain - a.gain);
  p.sigma = a.sigma + t * (b.sigma - a.sigma);
  return p;
}

sf::ModelConfig model_config(const catalog::Configuration& c) noexcept
{
  return {sf::Passband(c.passband), sf::Slope(c.slope),
          sf::DriveMode(c.drive), sf::FilterSubModel(c.submodel)};
}
struct CharacterFilter : sf::Filter
{
  // Access the exact computed floats, including SST's pitch and fasttan mapping.
  Design fast_svf_design(int response) const noexcept
  {
    const auto* c = payload.makers[0].C;
    const double a1 = c[0], a2 = c[1], a3 = c[2];
    const double m0 = response == LowPass ? 0. : c[3];
    const double m1 = response == LowPass ? 0. : c[4];
    const double m2 = response == LowPass ? 1. : c[5];
    const double a = 2. * a1 - 1., b = -2. * a2;
    const double cc = 2. * a2, d = 1. - 2. * a3;
    const double b0 = 2. * a2, b1 = 2. * a3;
    const double c0 = m1 * a1 + m2 * a2, c1 = -m1 * a2 + m2 * (1. - a3);
    const double direct = m0 + m1 * a2 + m2 * a3;
    const double trace = a + d, determinant = a * d - b * cc;
    Design result;
    result.count = 1;
    result.sections[0] = {direct, c0 * b0 + c1 * b1 - direct * trace,
        direct * determinant + c0 * (-d * b0 + b * b1) + c1 * (cc * b0 - a * b1),
        -trace, determinant};
    return result;
  }
  void clear_history(int lane) noexcept
  {
    // resetVoice also resets coefficients, and its upstream register mask clears
    // the other voices. Clear only this lane, retaining fixed coefficients.
    alignas(16) float values[4];
    for(auto& r : payload.qfuState.R)
    {
      SIMD_MM(store_ps)(values, r);
      values[lane] = 0.f;
      r = SIMD_MM(load_ps)(values);
    }
  }
  // Keeps the coefficients the kernels reached at the end of the last block.
  void freeze() noexcept
  {
    for(auto& maker : payload.makers)
      std::fill(std::begin(maker.dC), std::end(maker.dC), 0.f);
    prepareBlock();
  }
};

void warm_catalog()
{
  // Force all lazy allocating maps/tables before any realtime configuration.
  static const bool ready = [] {
    std::vector<float> delay(delay_size);
    for(const auto& model : catalog::models)
    {
      for(int i = 0; i < model.count; ++i)
      {
        CharacterFilter f;
        f.setMono();
        f.setSampleRateAndBlockSize(96000., sst_block);
        f.provideDelayLine(0, delay.data());
        f.setFilterModel(sf::FilterModel(model.sst_id));
        f.setModelConfiguration(model_config(catalog::configurations[model.begin + i]));
        if(!f.prepareInstance())
          continue;
        f.makeConstantCoefficients(0, 0.f, .2f, 1.f);
        f.prepareBlock();
        f.processMonoSample(0.f);
      }
    }
    return true;
  }();
  (void)ready;
}
struct SstControls
{
  float pitch{}, resonance{}, extra{};
};
SstControls sst_controls(const Parameters& p, const catalog::Configuration& c) noexcept
{
  // The Fast SVF extra is an amplitude (squared for bell/shelf gain), unlike
  // the bipolar normalized comb mix. Never run it through the catalog's GUI hint.
  const float extra = p.model == FastSVF && shelf(p.response)
      ? float(std::pow(10., p.gain / 40.)) : float(c.bipolar ? 2. * p.extra - 1. : p.extra);
  return {float(12. * std::log2(p.frequency / 440.)), float(p.resonance), extra};
}
// False when the catalog and the library disagree: the band passes through.
bool select_character(CharacterFilter& filter, const Parameters& p, double rate,
                      int channels, const std::array<float*, 4>& delay) noexcept
{
  const int index = character_configuration(p);
  if(index < 0 || index >= int(catalog::configurations.size()))
    return false;
  const auto& c = catalog::configurations[index];
  filter = CharacterFilter{};
  filter.setSampleRateAndBlockSize(rate, sst_block);
  for(int lane = 0; lane < 4; ++lane)
  {
    filter.setActive(lane, lane < channels);
    if(lane < channels && delay[lane])
      filter.provideDelayLine(lane, delay[lane]);
  }
  filter.setFilterModel(sf::FilterModel(catalog::models[p.model - VemberClassic].sst_id));
  filter.setModelConfiguration(model_config(c));
  if(!filter.prepareInstance())
    return false;
  const auto controls = sst_controls(p, c);
  filter.makeConstantCoefficients(0, controls.pitch, controls.resonance, controls.extra);
  for(int lane = 1; lane < channels; ++lane)
    filter.copyCoefficientsFromVoiceToVoice(0, lane);
  // All coefficient deltas are zero: until a retune, the kernels keep them.
  filter.prepareBlock();
  return true;
}

// Free comb delay lines. The vectors are reserved for every line the engine
// may hold, so that handing lines back and forth never allocates.
struct Pool
{
  std::vector<std::unique_ptr<float[]>> owned;
  std::vector<float*> free;
  void reserve(std::size_t capacity)
  {
    owned.reserve(capacity);
    free.reserve(capacity);
  }
  bool add(std::unique_ptr<float[]>& line) noexcept
  {
    if(!line || owned.size() == owned.capacity())
      return false;
    free.push_back(line.get());
    owned.push_back(std::move(line));
    return true;
  }
  float* take() noexcept
  {
    if(free.empty())
      return nullptr;
    auto* line = free.back();
    free.pop_back();
    return line;
  }
  void give(float* line) noexcept
  {
    if(line)
      free.push_back(line);
  }
};

struct Band
{
  struct History { double ic1{}, ic2{}; };
  Parameters parameters{};
  // Linear designs: the sections run, and where they move to.
  Sections design, target, delta;
  int design_left{};
  std::array<std::array<History, max_sections>, 4> history{};
  std::array<IvantsovFilter, 4> ivantsov;
  Parameters ivantsov_from{}, ivantsov_to{}, ivantsov_at{};
  int ivantsov_left{}, ivantsov_length{};
  CharacterFilter character;
  SstControls sst{};
  bool sst_moving{};
  int sst_position{}, sst_blocks{};
  std::array<float*, 4> lines{};
  int line_count{};
  Ramp drive, trim;
  bool comb{};
  void (*kernel)(Band&, Frame&, int) noexcept{};

  // Whether the running filter can move from a to b, keeping its histories:
  // the same structure, and for linear and Ivantsov designs a small enough
  // step; a larger one restarts the band in a crossfade. Each trapezoidal
  // state-variable section stays stable however fast its g and k move
  // (Wishnick, DAFx 2014), but states carried across a large step ring above
  // the levels of both designs. Measured on noise through every linear
  // design, a carried step peaks no higher than a crossfade up to half an
  // octave of frequency, Q or relative width and two octaves of the ripple's
  // epsilon, on which the Chebyshev poles depend, and higher beyond; a gain
  // step does not, up to the 48 dB the gains span.
  // SST smooths its own coefficients.
  static bool compatible(const Parameters& a, const Parameters& b) noexcept
  {
    if(a.enabled != b.enabled)
      return false;
    if(!a.enabled)
      return true;
    if(a.model != b.model)
      return false;
    const auto octaves = [](double x, double y) { return std::abs(std::log2(y / x)); };
    // (f2 - f1) / f for edges `bw` octaves apart around f: 2 sinh(bw ln 2 / 2).
    const auto relative = [](double bw) { return 2. * std::sinh(bw * std::numbers::ln2 / 2.); };
    const bool close = octaves(a.frequency, b.frequency) <= max_octaves
        && octaves(a.q, b.q) <= max_octaves
        && octaves(relative(a.bandwidth), relative(b.bandwidth)) <= max_octaves
        && std::abs(a.shelf_slope - b.shelf_slope) <= .1;
    // epsilon = sqrt(10^(ripple / 10) - 1)
    const auto epsilon = [](double db) { return std::sqrt(std::expm1(db * std::log(10.) / 10.)); };
    if(a.model < Ivantsov)
      return close && a.response == b.response && a.order == b.order
             && (a.model != ChebyshevI
                 || octaves(epsilon(a.ripple), epsilon(b.ripple)) <= max_epsilon_octaves);
    if(a.model == Ivantsov)
      return close && std::abs(a.gain - b.gain) <= max_db && std::abs(a.sigma - b.sigma) <= .1
             && a.response == b.response && a.order == b.order
             && a.correction == b.correction;
    return character_configuration(a) == character_configuration(b);
  }

  void release(Pool& pool) noexcept
  {
    for(int lane = 0; lane < line_count; ++lane)
      pool.give(std::exchange(lines[lane], nullptr));
    line_count = 0;
  }
  // Restarts the band from silence. A comb band takes one line per channel,
  // which the caller made sure the pool has.
  void configure(const Parameters& p, double rate, int channels, Pool& pool) noexcept
  {
    release(pool);
    parameters = p;
    history = {};
    drive.set(amplitude(p.drive));
    trim.set(amplitude(p.trim));
    design_left = ivantsov_left = 0;
    sst_moving = false;
    comb = false;
    kernel = nullptr;
    if(!p.enabled)
      return;
    if(p.model < Ivantsov)
    {
      design = target = state_variable(linear_design(p, rate));
      kernel = linear_kernel;
    }
    else if(p.model == Ivantsov)
    {
      for(int lane = 0; lane < channels; ++lane)
        ivantsov[lane].configure(p, rate);
      ivantsov_at = p;
      kernel = ivantsov_kernel;
    }
    else
    {
      // A filter that cannot be set up keeps only the band's gains.
      const auto unavailable = [&] {
        release(pool);
        comb = false;
        design = target = {};
        kernel = linear_kernel;
      };
      if(p.model == Comb)
      {
        comb = true;
        for(; line_count < channels; ++line_count)
        {
          lines[line_count] = pool.take();
          if(!lines[line_count])
            return unavailable();
          std::fill_n(lines[line_count], delay_size, 0.f);
        }
      }
      if(!select_character(character, p, rate, channels, lines))
        return unavailable();
      sst = sst_controls(p, catalog::configurations[character_configuration(p)]);
      kernel = character_kernel;
    }
  }
  // Moves the band to p over `length` samples, from wherever it is now and
  // keeping its histories. p must be compatible with the configured settings.
  void retune(const Parameters& p, double rate, int length) noexcept
  {
    drive.to(amplitude(p.drive), length);
    trim.to(amplitude(p.trim), length);
    if(kernel && p.enabled)
    {
      if(kernel == linear_kernel && p.model < Ivantsov)
      {
        const auto next = state_variable(linear_design(p, rate));
        if(next.count == design.count)
        {
          target = next;
          if(length <= 1)
          {
            design = target;
            design_left = 0;
          }
          else
          {
            for(int i = 0; i < design.count; ++i)
              delta.sections[i] = Section::step(design.sections[i], target.sections[i], length);
            design_left = length;
          }
        }
      }
      else if(kernel == ivantsov_kernel)
      {
        ivantsov_from = ivantsov_at;
        ivantsov_to = p;
        ivantsov_left = ivantsov_length = std::max(1, length);
      }
      else if(kernel == character_kernel)
      {
        sst = sst_controls(p, catalog::configurations[character_configuration(p)]);
        if(!sst_moving)
          sst_position = 0;
        sst_moving = true;
        sst_blocks = 0;
      }
    }
    parameters = p;
  }

  static void linear_kernel(Band& b, Frame& frame, int channels) noexcept
  {
    const double drive = b.drive.next(), trim = b.trim.next();
    if(b.design_left)
    {
      if(--b.design_left)
        for(int i = 0; i < b.design.count; ++i)
          b.design.sections[i].add(b.delta.sections[i]);
      else
        b.design = b.target;
    }
    for(int lane = 0; lane < channels; ++lane)
    {
      double x = frame[lane] * drive;
      for(int i = 0; i < b.design.count; ++i)
      {
        const auto& c = b.design.sections[i];
        auto& h = b.history[lane][i];
        const double v3 = x - h.ic2;
        const double v1 = c.a1 * h.ic1 + c.a2 * v3;
        const double v2 = h.ic2 + c.a2 * h.ic1 + c.a3 * v3;
        const double y = c.m0 * x + c.m1 * v1 + c.m2 * v2;
        if(!std::isfinite(y) || !std::isfinite(v1) || !std::isfinite(v2))
        {
          b.history[lane] = {};
          x = 0.;
          break;
        }
        const double ic1 = 2. * v1 - h.ic1, ic2 = 2. * v2 - h.ic2;
        h.ic1 = std::abs(ic1) < 1.e-30 ? 0. : ic1;
        h.ic2 = std::abs(ic2) < 1.e-30 ? 0. : ic2;
        x = y;
      }
      frame[lane] = x * trim;
    }
  }
  static void ivantsov_kernel(Band& b, Frame& frame, int channels) noexcept
  {
    const double drive = b.drive.next(), trim = b.trim.next();
    if(b.ivantsov_left && --b.ivantsov_left % ivantsov_step == 0)
    {
      b.ivantsov_at = interpolate(b.ivantsov_from, b.ivantsov_to,
          1. - double(b.ivantsov_left) / b.ivantsov_length);
      for(int lane = 0; lane < channels; ++lane)
        b.ivantsov[lane].set(b.ivantsov_at);
    }
    for(int lane = 0; lane < channels; ++lane)
      frame[lane] = b.ivantsov[lane].process(frame[lane] * drive) * trim;
  }
  static void character_kernel(Band& b, Frame& frame, int channels) noexcept
  {
    const double drive = b.drive.next(), trim = b.trim.next();
    if(b.sst_moving && b.sst_position == 0)
    {
      // SST smooths its coefficients towards these over the block.
      b.character.makeCoefficients(0, b.sst.pitch, b.sst.resonance, b.sst.extra);
      for(int lane = 1; lane < channels; ++lane)
        b.character.copyCoefficientsFromVoiceToVoice(0, lane);
      b.character.prepareBlock();
    }
    alignas(16) float in[4]{}, out[4];
    for(int lane = 0; lane < channels; ++lane)
    {
      const float x = float(frame[lane] * drive);
      in[lane] = std::isfinite(x) ? x : 0.f;
    }
    SIMD_MM(store_ps)(out, b.character.processSample(SIMD_MM(load_ps)(in)));
    for(int lane = 0; lane < channels; ++lane)
    {
      const bool valid = std::isfinite(out[lane]);
      frame[lane] = valid ? double(out[lane]) * trim : 0.;
      if(!valid)
      {
        b.character.clear_history(lane);
        if(b.comb && lane < b.line_count)
          std::fill_n(b.lines[lane], delay_size, 0.f);
      }
    }
    if(b.sst_moving && ++b.sst_position == sst_block)
    {
      b.character.concludeBlock();
      b.sst_position = 0;
      if(++b.sst_blocks == sst_settle_blocks)
      {
        b.character.freeze();
        b.sst_moving = false;
      }
    }
  }
};

struct Group
{
  std::array<Band, band_count> storage;
  // Swap handles after a fade, never the placement-constructed filter objects.
  // Both groups' storage lives at stable addresses until the next prepare.
  std::array<Band*, band_count> bands;
  std::array<hb::Interpolator2x, 4> up1, up2;
  std::array<hb::Decimator2x, 4> down1, down2;
  int channels{};
  Group() noexcept
  {
    for(int i = 0; i < band_count; ++i)
      bands[i] = &storage[i];
  }
  Group(const Group&) = delete;
  Group& operator=(const Group&) = delete;
  void reset_resamplers() noexcept
  {
    up1 = {}; up2 = {}; down1 = {}; down2 = {};
  }
  void chain(Frame& frame) noexcept
  {
    for(auto* band : bands)
      if(band->kernel)
        band->kernel(*band, frame, channels);
  }
  void fade_chain(Frame& frame, Group& target,
                  const std::array<bool, band_count>& changed, double mix) noexcept
  {
    for(int i = 0; i < band_count; ++i)
    {
      auto& band = *bands[i];
      if(changed[i])
      {
        auto next = frame;
        if(band.kernel)
          band.kernel(band, frame, channels);
        auto& destination = *target.bands[i];
        if(destination.kernel)
          destination.kernel(destination, next, channels);
        for(int lane = 0; lane < channels; ++lane)
          frame[lane] += mix * (next[lane] - frame[lane]);
      }
      else if(band.kernel)
        band.kernel(band, frame, channels);
    }
  }
  Frame process(const Frame& dry, int quality, Group* target = nullptr,
                const std::array<bool, band_count>* changed = nullptr,
                double mix = 0.) noexcept
  {
    if(quality == 0)
    {
      auto result = dry;
      if(target)
        fade_chain(result, *target, *changed, mix);
      else
        chain(result);
      return result;
    }
    std::array<Frame, 4> samples{};
    for(int lane = 0; lane < channels; ++lane)
    {
      const float converted = float(dry[lane]);
      const float x = std::isfinite(converted) ? converted : 0.f;
      float a, b;
      up1[lane].process(x, a, b);
      if(quality == 1)
      {
        samples[0][lane] = a; samples[1][lane] = b;
      }
      else
      {
        float aa, ab, ba, bb;
        up2[lane].process(a, aa, ab);
        up2[lane].process(b, ba, bb);
        samples[0][lane] = aa; samples[1][lane] = ab;
        samples[2][lane] = ba; samples[3][lane] = bb;
      }
    }
    for(int i = 0; i < (1 << quality); ++i)
      if(target)
        fade_chain(samples[i], *target, *changed, mix);
      else
        chain(samples[i]);
    Frame result{};
    for(int lane = 0; lane < channels; ++lane)
    {
      const auto value = [&](int i) noexcept {
        const float v = float(samples[i][lane]);
        return std::isfinite(v) ? v : 0.f;
      };
      if(quality == 1)
        result[lane] = down1[lane].process(value(0), value(1));
      else
      {
        const float a = down2[lane].process(value(0), value(1));
        const float b = down2[lane].process(value(2), value(3));
        result[lane] = down1[lane].process(a, b);
      }
      if(!std::isfinite(result[lane]))
      {
        up1[lane].reset(); up2[lane].reset();
        down1[lane].reset(); down2[lane].reset();
        result[lane] = 0.;
      }
    }
    return result;
  }
};
struct Bank
{
  Settings settings;
  std::vector<std::unique_ptr<Group>> groups;
};

// Each polyphase branch is a cascade of (a + z^-1)/(1 + a*z^-1)
// at the lower rate. Keep both images, including the odd-sample phase of the
// decimator, instead of multiplying only the principal passband magnitudes.
std::pair<Complex, Complex> halfband_branches(double angle) noexcept
{
  const auto z = std::polar(1., -angle);
  Complex even{1.}, odd{1.};
  for(std::size_t i = 0; i < hb::Chains::coefficients.size(); i += 2)
  {
    const double a = hb::Chains::coefficients[i], b = hb::Chains::coefficients[i + 1];
    even *= (a + z) / (1. + a * z);
    odd *= (b + z) / (1. + b * z);
  }
  return {even, odd};
}
Complex resampler_weight(double angle, int quality) noexcept
{
  Complex weight{1.};
  for(int stage = 0; stage < quality; ++stage)
  {
    const auto [even, odd] = halfband_branches(2. * angle);
    const auto z = std::polar(1., -angle);
    weight *= .25 * (even + z * odd) * (odd + std::conj(z) * even);
    angle *= 2.;
  }
  return weight;
}
} // namespace

struct Engine::State
{
  std::array<Bank, 2> banks;
  Pool pool;
  Settings requested;
  double rate{};
  int channels{}, active{}, remaining{}, fade_length{};
  int shortest_ramp{}, longest_ramp{}, missing{};
  bool pending{}, quality_change{};
  std::array<bool, band_count> changed{};

  double internal_rate(int quality) const noexcept { return rate * (1 << quality); }
  int lines(const Parameters& p) const noexcept
  {
    return p.enabled && p.model == Comb ? channels : 0;
  }
  int held(const Bank& bank, int band) const noexcept
  {
    int n{};
    for(const auto& group : bank.groups)
      n += group->bands[band]->line_count;
    return n;
  }
  void release(Bank& bank) noexcept
  {
    for(auto& group : bank.groups)
      for(auto* band : group->bands)
        band->release(pool);
  }
  // Not real-time: comb lines for these settings, `copies` times.
  void reserve(const Settings& settings, int copies)
  {
    std::size_t needed{};
    for(const auto& p : settings.bands)
      needed += std::size_t(copies * lines(p));
    needed = std::min(needed, pool.owned.capacity());
    while(pool.owned.size() < needed)
    {
      std::unique_ptr<float[]> line{new float[delay_size]};
      pool.add(line);
    }
  }
  // Restarts every band of the bank. A comb band the pool has no lines for
  // is left disabled, and the bank's settings say so.
  void configure(Bank& bank, const Settings& settings) noexcept
  {
    release(bank);
    bank.settings = settings;
    for(auto& group : bank.groups)
      group->reset_resamplers();
    int deficit{};
    for(int i = 0; i < band_count; ++i)
    {
      auto& p = bank.settings.bands[i];
      if(const int need = lines(p); need > int(pool.free.size()))
      {
        deficit += need;
        p.enabled = false;
      }
      for(auto& group : bank.groups)
        group->bands[i]->configure(p, internal_rate(settings.quality), group->channels, pool);
    }
    missing = std::max(missing, deficit);
  }
  bool configure(Bank& bank, int band, const Parameters& p) noexcept
  {
    const int need = lines(p), available = int(pool.free.size()) + held(bank, band);
    if(need > available)
    {
      missing = std::max(missing, need - available);
      return false;
    }
    for(auto& group : bank.groups)
      group->bands[band]->configure(p, internal_rate(bank.settings.quality), group->channels, pool);
    return true;
  }

  // frames: what is left of the block, over which a continuous change moves.
  void begin(int frames) noexcept
  {
    if(!pending || (remaining && quality_change))
      return;
    auto& current = banks[active];
    auto& next = banks[1 - active];
    if(!remaining && requested.quality != current.settings.quality)
    {
      configure(next, requested);
      quality_change = true;
      remaining = fade_length;
      pending = false;
      return;
    }
    const int quality = current.settings.quality;
    const int length = std::clamp(frames, shortest_ramp, longest_ramp) << quality;
    if(!remaining)
      changed.fill(false);
    bool waiting = false, start = false;
    for(int i = 0; i < band_count; ++i)
    {
      // A band fading towards a configuration keeps moving from that one.
      auto& owner = remaining && changed[i] ? next : current;
      auto& applied = owner.settings.bands[i];
      const auto& wanted = requested.bands[i];
      if(wanted == applied)
        continue;
      if(Band::compatible(applied, wanted))
      {
        for(auto& group : owner.groups)
          group->bands[i]->retune(wanted, internal_rate(quality), length);
        applied = wanted;
      }
      else if(remaining)
        waiting = true;
      else
      {
        next.settings.quality = quality;
        if(configure(next, i, wanted))
        {
          next.settings.bands[i] = wanted;
          changed[i] = start = true;
        }
        else
          waiting = true;
      }
    }
    if(start)
    {
      quality_change = false;
      remaining = fade_length;
    }
    pending = waiting;
  }
  void finish() noexcept
  {
    auto& current = banks[active];
    auto& next = banks[1 - active];
    if(quality_change)
    {
      release(current);
      active = 1 - active;
      quality_change = false;
    }
    else
    {
      // Unedited bands and the shared resampler never restart or run twice.
      // Kernels and delay buffers retain their addresses throughout the swap.
      for(std::size_t g = 0; g < current.groups.size(); ++g)
        for(int i = 0; i < band_count; ++i)
          if(changed[i])
          {
            std::swap(current.groups[g]->bands[i], next.groups[g]->bands[i]);
            next.groups[g]->bands[i]->release(pool);
          }
      for(int i = 0; i < band_count; ++i)
        if(changed[i])
          current.settings.bands[i] = next.settings.bands[i];
    }
    changed.fill(false);
    // Requests that waited for the fade, or for comb storage, start now.
    pending = true;
  }
};

Engine::Engine() = default;
Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

Engine::Storage Engine::allocate(int lines)
{
  Storage storage;
  storage.lines.reserve(std::size_t(std::max(0, lines)));
  for(int i = 0; i < lines; ++i)
    storage.lines.emplace_back(new float[delay_size]);
  return storage;
}

Engine Engine::prepare(double rate, int channels, const Settings& settings, bool transitions)
{
  rate = sample_rate(rate);
  channels = std::max(0, channels);
  const int copies = transitions ? 2 : 1;
  if(state && state->rate == rate && state->channels == channels)
  {
    update(settings);
    state->reserve(state->requested, copies);
    return {};
  }
  warm_catalog();
  auto next = std::make_unique<State>();
  next->rate = rate;
  next->channels = channels;
  next->requested = sanitize(settings, rate);
  next->fade_length = std::max(1, int(std::ceil(rate * .01)));
  next->shortest_ramp = std::max(1, int(std::ceil(rate * .001)));
  next->longest_ramp = std::max(1, int(std::ceil(rate * .02)));
  next->pool.reserve(std::size_t(2 * band_count * channels));
  next->reserve(next->requested, copies);
  for(auto& bank : next->banks)
  {
    bank.groups.reserve((channels + 3) / 4);
    for(int channel = 0; channel < channels; channel += 4)
    {
      auto group = std::make_unique<Group>();
      group->channels = std::min(4, channels - channel);
      bank.groups.push_back(std::move(group));
    }
  }
  next->configure(next->banks[0], next->requested);
  next->pending = true;
  Engine retired;
  retired.state = std::exchange(state, std::move(next));
  return retired;
}
void Engine::update(const Settings& settings) noexcept
{
  if(state)
  {
    // The reference level is the graph's measurement level: no band uses it.
    const auto requested = sanitize(settings, state->rate);
    if(requested != state->requested)
    {
      state->requested = requested;
      state->pending = true;
    }
  }
}
int Engine::missing_storage() const noexcept
{
  return state ? state->missing : 0;
}
void Engine::adopt(Storage& storage) noexcept
{
  if(!state)
    return;
  for(auto& line : storage.lines)
    state->pool.add(line);
  state->missing = 0;
  state->pending = true;
}
void Engine::reset() noexcept
{
  if(!state)
    return;
  auto& s = *state;
  s.active = s.remaining = 0;
  s.quality_change = false;
  s.changed.fill(false);
  s.release(s.banks[1]);
  s.configure(s.banks[0], s.requested);
  s.pending = true;
}
void Engine::process(const double* const* input, int input_channels,
                     double* const* output, int output_channels, int frames) noexcept
{
  if(frames <= 0 || !output || output_channels <= 0)
    return;
  if(!state)
  {
    for(int c = 0; c < output_channels; ++c)
      if(output[c])
        std::fill_n(output[c], frames, 0.);
    return;
  }
  auto& s = *state;
  for(int i = 0; i < frames; ++i)
  {
    s.begin(frames - i);
    const bool fading = s.remaining > 0;
    const double mix = fading ? double(s.fade_length - s.remaining + 1) / s.fade_length : 0.;
    auto& current = s.banks[s.active];
    auto& next = s.banks[1 - s.active];
    int channel{};
    for(std::size_t g = 0; g < current.groups.size(); ++g)
    {
      auto& group = *current.groups[g];
      Frame dry{};
      for(int lane = 0; lane < group.channels; ++lane)
      {
        const int c = channel + lane;
        const double x = input && c < input_channels && input[c] ? input[c][i] : 0.;
        dry[lane] = std::isfinite(x) ? x : 0.;
      }
      const bool band_fade = fading && !s.quality_change;
      auto result = group.process(dry, current.settings.quality,
          band_fade ? next.groups[g].get() : nullptr,
          band_fade ? &s.changed : nullptr, mix);
      if(fading && s.quality_change)
      {
        const auto target = next.groups[g]->process(dry, next.settings.quality);
        for(int lane = 0; lane < group.channels; ++lane)
          result[lane] = (1. - mix) * result[lane] + mix * target[lane];
      }
      for(int lane = 0; lane < group.channels; ++lane)
      {
        const int c = channel + lane;
        if(c < output_channels && output[c])
          output[c][i] = std::isfinite(result[lane]) ? result[lane] : 0.;
      }
      channel += group.channels;
    }
    for(int c = s.channels; c < output_channels; ++c)
      if(output[c])
        output[c][i] = 0.;
    if(fading && --s.remaining == 0)
      s.finish();
  }
}

bool analytic_response(const Settings& settings, double rate,
                       std::span<const double> frequencies, std::span<double> db)
{
  if(db.size() != frequencies.size())
    return false;
  rate = sample_rate(rate);
  const auto target = sanitize(settings, rate);
  if(response_kind(target) != ResponseKind::Analytic)
    return false;
  warm_catalog();
  std::array<Design, band_count> designs;
  std::array<IvantsovFilter, band_count> ivantsov;
  double gain{1.};
  const int factor = 1 << target.quality;
  const double internal_rate = rate * factor;
  for(int i = 0; i < band_count; ++i)
  {
    const auto& p = target.bands[i];
    if(!p.enabled)
      continue;
    gain *= amplitude(p.drive + p.trim);
    if(p.model < Ivantsov)
      designs[i] = linear_design(p, internal_rate);
    else if(p.model == Ivantsov)
    {
      ivantsov[i].configure(p, internal_rate);
      ivantsov[i].capture_response();
    }
    else if(p.model == FastSVF)
    {
      CharacterFilter filter;
      if(select_character(filter, p, internal_rate, 1, {}))
        designs[i] = filter.fast_svf_design(p.response);
    }
    else
      return false;
  }
  for(std::size_t j = 0; j < frequencies.size(); ++j)
  {
    const double frequency = frequencies[j];
    if(!std::isfinite(frequency) || frequency < 0. || frequency > rate * .5)
      return false;
    Complex total{};
    for(int image = 0; image < factor; ++image)
    {
      const double angle = (2. * pi * frequency / rate + 2. * pi * image) / factor;
      const double signed_angle = std::remainder(angle, 2. * pi);
      const double hz = signed_angle * internal_rate / (2. * pi);
      Complex response{gain};
      for(int i = 0; i < band_count; ++i)
      {
        const auto& p = target.bands[i];
        if(p.enabled)
          response *= p.model == Ivantsov ? ivantsov[i].response(hz)
                                         : designs[i].response(hz, internal_rate);
      }
      total += resampler_weight(angle, target.quality) * response;
    }
    const double magnitude = std::abs(total);
    if(!std::isfinite(magnitude))
      return false;
    db[j] = 20. * std::log10(std::max(1.e-30, magnitude));
  }
  return true;
}
} // namespace examples::parametriq
