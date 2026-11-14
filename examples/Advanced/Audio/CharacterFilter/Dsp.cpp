/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "Dsp.hpp"
#include "../DynamicWaveshaper/Halfband.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>
#include <vector>
#include <sst/filters++.h>

namespace ao::character_filter
{
namespace
{
namespace sf = sst::filtersplusplus;
namespace hb = ao::dynamic_waveshaper::halfband;
using Sinc = sst::filters::utilities::SincTable;
constexpr int quantum = 16; // Host samples; SST processes 32 at twice the host rate.
// SST's comb ring (COMBquad_SSE2<MAX_FB_COMB>) wraps its write position at
// MAX_FB_COMB whatever the delay time and mirrors its first FIRipol_N samples
// after the end: Filter::requiredDelayLinesSizes is this size, at any rate.
constexpr int comb_size = sst::filters::utilities::MAX_FB_COMB;
constexpr int delay_size = comb_size + Sinc::FIRipol_N;
// Filter input bound: at +36 dB drive it stays far within single precision.
constexpr double input_limit = 64.;
// A filter output beyond this, or a non-finite one, means its state diverged.
constexpr float divergence_limit = 1.e8f;

sf::ModelConfig modelConfig(const Configuration& c)
{
  return {sf::Passband(c.passband), sf::Slope(c.slope),
          sf::DriveMode(c.drive), sf::FilterSubModel(c.submodel)};
}

// Configurations whose coefficients SST designs as a normalized lattice: its
// k1 is the single-precision cosine of the cutoff angle, and q1 = sqrt(1 - k1^2)
// divides the output taps. Below 2^-12 rad that cosine rounds to 1 and the
// coefficients are infinite. The bound keeps it at least two ulps below 1.
bool lattice(int configuration) noexcept
{
  const auto& c = configurations[configuration];
  const auto classic = std::find_if(models.begin(), models.end(), [](const Model& m) {
    return m.sst_id == int(sf::FilterModel::VemberClassic);
  });
  if(classic == models.end() || configuration < classic->begin
     || configuration >= classic->begin + classic->count)
    return false;
  return sf::DriveMode(c.drive) == sf::DriveMode::Clean || sf::Passband(c.passband) == sf::Passband::Notch
         || sf::Passband(c.passband) == sf::Passband::Allpass;
}
constexpr double lattice_angle = 0x1p-11;

std::array<bool, configurations.size()> available{};

// Initialize SST's allocating configuration maps before any audio processing.
// No configuration enumeration, strings or lazy table initialization in process().
// A configuration the pinned SST rejects stays unavailable.
const bool catalog_ready = [] {
  std::vector<float> delay(delay_size);
  for(const auto& model : models)
  {
    for(int i = 0; i < model.count; ++i)
    {
      sf::Filter f;
      f.setMono();
      f.setSampleRateAndBlockSize(96000., 2 * quantum);
      f.provideDelayLine(0, delay.data());
      f.setFilterModel(sf::FilterModel(model.sst_id));
      f.setModelConfiguration(modelConfig(configurations[model.begin + i]));
      if(!f.prepareInstance())
        continue;
      available[model.begin + i] = true;
      f.makeCoefficients(0, 0.f, 0.2f);
      f.prepareBlock();
      for(int j = 0; j < 2 * quantum; ++j)
        f.processMonoSample(0.f);
      f.concludeBlock();
    }
  }
  return true;
}();

float bounded(float x, float lo, float hi, float fallback) noexcept
{
  return std::isfinite(x) ? std::clamp(x, lo, hi) : fallback;
}
Parameters sanitize(Parameters p, double rate) noexcept
{
  p.model = std::clamp(p.model, 0, int(models.size()) - 1);
  p.variant = std::clamp(p.variant, 0, models[p.model].count - 1);
  p.cutoff = bounded(p.cutoff, 20.f, float(std::min(20000., rate * 0.45)), 1000.f);
  p.resonance = bounded(p.resonance, 0.f, 1.f, 0.2f);
  p.drive_db = bounded(p.drive_db, -24.f, 36.f, 0.f);
  p.output_db = bounded(p.output_db, -60.f, 12.f, 0.f);
  p.extra = bounded(p.extra, 0.f, 1.f, 0.5f);
  return p;
}
float approach(float value, float target, float coefficient) noexcept
{
  const float delta = target - value;
  return std::abs(delta) < 1.e-6f ? target : value + coefficient * delta;
}
float pitchOf(float hz) noexcept
{
  return 12.f * std::log2(hz / 440.f);
}

struct Bank
{
  sf::Filter filter;
  // Only the comb reads it; see select() for what must be cleared.
  std::unique_ptr<float[]> delay;
  int model{}, variant{}, configuration{};
  float lowest_pitch{}, extra{};
  float previous_pitch{}, previous_resonance{}, previous_extra{};
  bool first_block{true}, diverged{};

  void prepare(int channels)
  {
    delay.reset(new float[std::size_t(channels) * delay_size]);
  }
  void select(double rate, int channels, int m, int v, float initial_extra) noexcept
  {
    model = m;
    variant = v;
    configuration = models[m].begin + v;
    extra = initial_extra;
    first_block = true;
    diverged = false;
    const float lowest = minimumCutoff(configuration, rate);
    lowest_pitch = pitchOf(lowest);
    // SST reset() does not reset the comb write pointers. Fresh state also
    // prevents histories being reinterpreted when changing filter topology.
    filter = sf::Filter{};
    filter.setSampleRateAndBlockSize(2. * rate, 2 * quantum);
    for(int c = 0; c < 4; ++c)
    {
      filter.setActive(c, c < channels);
      if(c < channels)
        filter.provideDelayLine(c, delay.get() + c * delay_size);
    }
    const bool valid = available[configuration];
    if(valid && models[m].sst_id == int(sf::FilterModel::Comb))
    {
      // A new filter writes from position 0 on. Each sample reads FIRipol_N taps
      // from (write - DT - FIRoffset) modulo comb_size, with FIRipol_N <= DT
      // <= ceil(dtime) and dtime = internal rate / cutoff. Until the writes
      // have gone round once, unwrapped reads lie behind the write position,
      // and wrapped ones within the last DT + FIRoffset samples of the ring
      // and its mirror: only those may hold stale data.
      const double longest = 2. * rate / lowest;
      const int stale = std::min(delay_size, int(std::ceil(longest * 1.001)) + 1
                                                 + Sinc::FIRoffset + Sinc::FIRipol_N);
      for(int c = 0; c < channels; ++c)
        std::fill_n(delay.get() + (c + 1) * delay_size - stale, stale, 0.f);
    }
    filter.setFilterModel(valid ? sf::FilterModel(models[m].sst_id) : sf::FilterModel::None);
    if(valid)
      filter.setModelConfiguration(modelConfig(configurations[configuration]));
    const bool ok = filter.prepareInstance();
    assert(ok);
    (void)ok;
  }
  void begin(int channels, float pitch, float resonance, float target_extra,
             float slew) noexcept
  {
    const auto& config = configurations[configuration];
    pitch = std::max(pitch, lowest_pitch);
    extra = approach(extra, target_extra, slew);
    if(first_block || pitch != previous_pitch || resonance != previous_resonance
       || extra != previous_extra)
    {
      filter.makeCoefficients(0, pitch, resonance, config.bipolar ? 2.f * extra - 1.f : extra);
      previous_pitch = pitch;
      previous_resonance = resonance;
      previous_extra = extra;
      first_block = false;
    }
    else
      filter.freezeCoefficientsFor(0);
    for(int c = 1; c < channels; ++c)
      filter.copyCoefficientsFromVoiceToVoice(0, c);
    filter.prepareBlock();
  }
  // Zero for the rest of the quantum once any active lane diverges; the
  // bank restarts from a fresh state at the next quantum.
  SIMD_M128 process(SIMD_M128 in, int lanes) noexcept
  {
    if(diverged)
      return SIMD_MM(setzero_ps)();
    const auto out = filter.processSample(in);
    const auto magnitude = SIMD_MM(andnot_ps)(SIMD_MM(set1_ps)(-0.f), out);
    const int sane = SIMD_MM(movemask_ps)(
        SIMD_MM(cmplt_ps)(magnitude, SIMD_MM(set1_ps)(divergence_limit)));
    if((sane & lanes) == lanes)
      return out;
    diverged = true;
    return SIMD_MM(setzero_ps)();
  }
};
struct Group
{
  Bank banks[2];
  std::array<hb::Interpolator2x, 4> up;
  std::array<hb::Decimator2x, 4> down;
  int channels{};
};
}

bool configurationAvailable(int configuration) noexcept
{
  (void)catalog_ready;
  return configuration >= 0 && configuration < int(configurations.size())
         && available[configuration];
}

float minimumCutoff(int configuration, double rate) noexcept
{
  constexpr float lowest = 20.f;
  if(configuration < 0 || configuration >= int(configurations.size()) || !lattice(configuration))
    return lowest;
  return std::max(lowest, float(lattice_angle * rate / std::numbers::pi));
}

struct Engine::State
{
  std::vector<std::unique_ptr<Group>> groups;
  Parameters requested{};
  // The latest extra of each model; a bank follows its own model's value.
  std::array<float, models.size()> extras{};
  double rate{};
  int channels{}, active{}, position{}, fade_remaining{}, fade_length{};
  int current_model{}, current_variant{}, next_model{}, next_variant{};
  float pitch{}, target_pitch{}, resonance{};
  float drive{}, target_drive{}, output{}, target_output{}, wet{};
  float control_slew{}, gain_slew{};

  void parameters(Parameters p) noexcept
  {
    p = sanitize(p, rate);
    if(p == requested)
      return;
    if(p.cutoff != requested.cutoff)
      target_pitch = pitchOf(p.cutoff);
    if(p.drive_db != requested.drive_db)
      target_drive = std::pow(10.f, p.drive_db / 20.f);
    if(p.output_db != requested.output_db)
      target_output = std::pow(10.f, p.output_db / 20.f);
    extras[p.model] = p.extra;
    requested = p;
  }
  void restart(Group& g, Bank& b) noexcept
  {
    b.select(rate, g.channels, b.model, b.variant, extras[b.model]);
  }
  void begin() noexcept
  {
    if(!fade_remaining && (current_model != requested.model
                          || current_variant != requested.variant))
    {
      next_model = requested.model;
      next_variant = requested.variant;
      for(auto& g : groups)
        g->banks[1 - active].select(rate, g->channels, next_model, next_variant,
                                    extras[next_model]);
      fade_remaining = fade_length;
    }
    pitch = approach(pitch, target_pitch, control_slew);
    resonance = approach(resonance, requested.resonance, control_slew);
    for(auto& g : groups)
    {
      auto& current = g->banks[active];
      if(current.diverged)
        restart(*g, current);
      current.begin(g->channels, pitch, resonance, extras[current.model], control_slew);
      if(fade_remaining)
      {
        auto& next = g->banks[1 - active];
        if(next.diverged)
          restart(*g, next);
        next.begin(g->channels, pitch, resonance, extras[next.model], control_slew);
      }
    }
  }
};

Engine::Engine() noexcept = default;
Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

void Engine::prepare(double rate, int channels, Parameters p)
{
  (void)catalog_ready;
  state = std::make_unique<State>();
  auto& s = *state;
  s.rate = std::isfinite(rate) && rate >= 8000. ? rate : 48000.;
  s.channels = std::max(0, channels);
  s.requested = sanitize(p, s.rate);
  s.extras.fill(Parameters{}.extra);
  s.extras[s.requested.model] = s.requested.extra;
  s.current_model = s.requested.model;
  s.current_variant = s.requested.variant;
  s.pitch = s.target_pitch = pitchOf(s.requested.cutoff);
  s.resonance = s.requested.resonance;
  s.drive = s.target_drive = std::pow(10.f, s.requested.drive_db / 20.f);
  s.output = s.target_output = std::pow(10.f, s.requested.output_db / 20.f);
  s.wet = s.requested.enabled ? 1.f : 0.f;
  s.control_slew = float(-std::expm1(-quantum / (s.rate * 0.005)));
  s.gain_slew = float(-std::expm1(-1. / (s.rate * 0.005)));
  s.fade_length = quantum * std::max(1, int(std::ceil(s.rate * 0.01 / quantum)));
  s.groups.reserve((s.channels + 3) / 4);
  for(int c = 0; c < s.channels; c += 4)
  {
    auto g = std::make_unique<Group>();
    g->channels = std::min(4, s.channels - c);
    for(auto& b : g->banks)
    {
      b.prepare(g->channels);
      b.select(s.rate, g->channels, s.current_model, s.current_variant,
               s.requested.extra);
    }
    s.groups.push_back(std::move(g));
  }
}

void Engine::setParameters(Parameters p) noexcept
{
  if(state)
    state->parameters(p);
}

void Engine::process(const double* const* input, int input_channels,
                     double* const* output, int output_channels, int frames) noexcept
{
  if(frames <= 0 || !output)
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
    if(s.position == 0)
      s.begin();
    s.drive = approach(s.drive, s.target_drive, s.gain_slew);
    s.output = approach(s.output, s.target_output, s.gain_slew);
    s.wet = approach(s.wet, s.requested.enabled ? 1.f : 0.f, s.gain_slew);
    const bool fading = s.fade_remaining != 0;
    const auto mix = SIMD_MM(set1_ps)(fading ? 1.f - float(s.fade_remaining) / s.fade_length : 0.f);
    int channel = 0;
    for(auto& group : s.groups)
    {
      auto& g = *group;
      const int lanes = (1 << g.channels) - 1;
      // The dry signal is mixed at the internal rate and decimated with the
      // filtered one, so that it has the same delay and phase response as the
      // wet path: the enable fade does not comb filter, and bypass keeps the
      // latency of the oversampling.
      alignas(16) float first[4]{}, second[4]{}, y1[4], y2[4];
      float dry1[4]{}, dry2[4]{};
      for(int lane = 0; lane < g.channels; ++lane)
      {
        const int c = channel + lane;
        const double x = input && c < input_channels && input[c] ? input[c][i] : 0.;
        const float bounded = float(std::isfinite(x) ? std::clamp(x, -input_limit, input_limit) : 0.);
        g.up[lane].process(bounded, dry1[lane], dry2[lane]);
        first[lane] = dry1[lane] * s.drive;
        second[lane] = dry2[lane] * s.drive;
      }
      auto run = [&](float* x, float* y) {
        const auto in = SIMD_MM(load_ps)(x);
        auto out = g.banks[s.active].process(in, lanes);
        if(fading)
        {
          const auto next = g.banks[1 - s.active].process(in, lanes);
          out = SIMD_MM(add_ps)(out, SIMD_MM(mul_ps)(mix, SIMD_MM(sub_ps)(next, out)));
        }
        SIMD_MM(store_ps)(y, out);
      };
      run(first, y1);
      run(second, y2);
      for(int lane = 0; lane < g.channels; ++lane)
      {
        float a = dry1[lane], b = dry2[lane];
        if(s.wet != 0.f)
        {
          a = s.wet * (y1[lane] * s.output) + (1.f - s.wet) * a;
          b = s.wet * (y2[lane] * s.output) + (1.f - s.wet) * b;
        }
        const float mixed = g.down[lane].process(a, b);
        const int c = channel + lane;
        if(c < output_channels && output[c])
          output[c][i] = mixed;
      }
      channel += g.channels;
    }
    if(++s.position == quantum)
    {
      for(auto& g : s.groups)
      {
        g->banks[s.active].filter.concludeBlock();
        if(fading)
          g->banks[1 - s.active].filter.concludeBlock();
      }
      s.position = 0;
    }
    if(fading && --s.fade_remaining == 0)
    {
      assert(s.position == 0);
      s.active = 1 - s.active;
      s.current_model = s.next_model;
      s.current_variant = s.next_variant;
    }
  }
  for(int c = s.channels; c < output_channels; ++c)
    if(output[c])
      std::fill_n(output[c], frames, 0.);
}
}
