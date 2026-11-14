#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "Halfband.hpp"
#include "SurgeShaper.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace ao::dynamic_waveshaper
{
// Integer inspection remains meaningful with -ffinite-math-only / -ffast-math.
inline bool finite(float value) noexcept
{
  return (std::bit_cast<std::uint32_t>(value) & 0x7f800000u) != 0x7f800000u;
}
inline bool finite(double value) noexcept
{
  return (std::bit_cast<std::uint64_t>(value) & 0x7ff0000000000000ull)
         != 0x7ff0000000000000ull;
}
inline float sanitize(float value, float low, float high, float fallback) noexcept
{
  return finite(value) ? std::clamp(value, low, high) : fallback;
}
inline float audioSample(float value, float limit = 64.f) noexcept
{
  return sanitize(value, -limit, limit, 0.f);
}
inline float flushSmall(float value) noexcept
{
  return std::abs(value) < 1.e-20f ? 0.f : value;
}
inline float dbToGain(float db) noexcept { return std::pow(10.f, db * 0.05f); }

struct FilterParameters
{
  bool highpass_enabled = false;
  bool lowpass_enabled = false;
  float highpass_hz = 20.f;
  float lowpass_hz = 20000.f;
};

struct BandParameters
{
  bool enabled = true;
  bool begin_enabled = false;
  bool end_enabled = false;
  bool dynamics_enabled = false;
  float begin_hz = 250.f;
  float end_hz = 2500.f;
  float drive_db = 0.f;
  float level_db = 0.f;
  float attack_ms = 10.f;
  float release_ms = 100.f;
  float threshold_db = -18.f;
  float ratio = 4.f;
  int algorithm = 0;
  FilterParameters pre_filters, post_filters;
};

struct Parameters
{
  std::array<BandParameters, 3> bands = [] {
    std::array<BandParameters, 3> result{};
    result[0].end_enabled = true;
    result[0].end_hz = 250.f;
    result[1].begin_enabled = true;
    result[1].end_enabled = true;
    result[2].begin_enabled = true;
    result[2].begin_hz = 2500.f;
    return result;
  }();
  // Rows are destinations, columns are sources.
  std::array<float, 9> matrix{};
  bool feedback_enabled = false;
  float output_db = 0.f;
  float input_db = 0.f;
  FilterParameters input_filters;
};

inline FilterParameters sanitize(FilterParameters p) noexcept
{
  p.highpass_hz = sanitize(p.highpass_hz, 20.f, 20000.f, 20.f);
  p.lowpass_hz = sanitize(p.lowpass_hz, 20.f, 20000.f, 20000.f);
  return p;
}
inline BandParameters sanitize(BandParameters p) noexcept
{
  p.begin_hz = sanitize(p.begin_hz, 20.f, 20000.f, 250.f);
  p.end_hz = sanitize(p.end_hz, 20.f, 20000.f, 2500.f);
  p.drive_db = sanitize(p.drive_db, -24.f, 36.f, 0.f);
  p.level_db = sanitize(p.level_db, -60.f, 12.f, 0.f);
  p.attack_ms = sanitize(p.attack_ms, 0.1f, 200.f, 10.f);
  p.release_ms = sanitize(p.release_ms, 5.f, 2000.f, 100.f);
  p.threshold_db = sanitize(p.threshold_db, -60.f, 0.f, -18.f);
  p.ratio = sanitize(p.ratio, 1.f, 20.f, 4.f);
  p.pre_filters = sanitize(p.pre_filters);
  p.post_filters = sanitize(p.post_filters);
  if(p.algorithm < 0 || p.algorithm >= algorithm_count)
    p.algorithm = 0;
  return p;
}
inline Parameters sanitize(Parameters p) noexcept
{
  for(auto& band : p.bands)
    band = sanitize(band);
  for(auto& route : p.matrix)
    route = sanitize(route, -1.f, 1.f, 0.f);
  p.output_db = sanitize(p.output_db, -60.f, 12.f, 0.f);
  p.input_db = sanitize(p.input_db, -24.f, 36.f, 0.f);
  p.input_filters = sanitize(p.input_filters);
  return p;
}

struct LinearRamp
{
  float value = 0.f;
  float target = 0.f;
  float step = 0.f;
  int remaining = 0;

  void reset(float v) noexcept
  {
    value = target = v;
    step = 0.f;
    remaining = 0;
  }
  void setTarget(float v, int samples) noexcept
  {
    // Repeated block parameter snapshots must not restart a running ramp.
    if(v == target)
      return;
    if(samples <= 0)
      return reset(v);
    target = v;
    remaining = samples;
    step = (target - value) / float(samples);
  }
  float next() noexcept
  {
    if(remaining > 0)
    {
      value += step;
      if(--remaining == 0)
        value = target;
    }
    return value;
  }
};

struct Lowpass
{
  LinearRamp coefficient;
  float state = 0.f;

  static double effectiveFrequency(float frequency, double rate) noexcept
  {
    // rate is the internal 2x rate; leave transition room below host Nyquist.
    return std::clamp(double(frequency), 1., rate * 0.225);
  }
  static float coefficientFor(float frequency, double rate) noexcept
  {
    const double hz = effectiveFrequency(frequency, rate);
    const double g = std::tan(std::numbers::pi * hz / rate);
    return float(g / (1. + g));
  }
  void prepare(float frequency, double rate) noexcept
  {
    coefficient.reset(coefficientFor(frequency, rate));
    state = 0.f;
  }
  void setFrequency(float frequency, double rate, int samples) noexcept
  {
    coefficient.setTarget(coefficientFor(frequency, rate), samples);
  }
  float process(float input) noexcept
  {
    if(!finite(state))
      state = 0.f;
    const float v = (input - state) * coefficient.next();
    const float output = v + state;
    state = flushSmall(output + v);
    return output;
  }
};

// Two cascaded non-resonant one-poles give each endpoint a 12 dB/oct slope.
// Unlike the complementary extraction filters, these endpoints are serial.
struct FilterPair
{
  std::array<Lowpass, 2> highpass, lowpass;
  LinearRamp highpassMix, lowpassMix;
  FilterParameters parameters;
  double rate = 96000.;
  int rampSamples = 480;

  void prepare(const FilterParameters& requested, double sampleRate) noexcept
  {
    parameters = sanitize(requested);
    rate = sampleRate;
    rampSamples = std::max(1, int(rate * 0.005));
    highpass[0].prepare(parameters.highpass_hz, rate);
    highpass[1] = highpass[0];
    lowpass[0].prepare(parameters.lowpass_hz, rate);
    lowpass[1] = lowpass[0];
    highpassMix.reset(parameters.highpass_enabled ? 1.f : 0.f);
    lowpassMix.reset(parameters.lowpass_enabled ? 1.f : 0.f);
  }
  void setParameters(const FilterParameters& requested) noexcept
  {
    const auto next = sanitize(requested);
    if(next.highpass_hz != parameters.highpass_hz)
    {
      const float coefficient = Lowpass::coefficientFor(next.highpass_hz, rate);
      for(auto& filter : highpass)
        filter.coefficient.setTarget(coefficient, rampSamples);
    }
    if(next.lowpass_hz != parameters.lowpass_hz)
    {
      const float coefficient = Lowpass::coefficientFor(next.lowpass_hz, rate);
      for(auto& filter : lowpass)
        filter.coefficient.setTarget(coefficient, rampSamples);
    }
    highpassMix.setTarget(next.highpass_enabled ? 1.f : 0.f, rampSamples);
    lowpassMix.setTarget(next.lowpass_enabled ? 1.f : 0.f, rampSamples);
    parameters = next;
  }
  float process(float input) noexcept
  {
    input = audioSample(input, 1.e6f);
    // Keep both histories warm even at exact bypass. The second endpoint
    // follows the actual (possibly crossfading) output of the first.
    float high = input - highpass[0].process(input);
    high -= highpass[1].process(high);
    const float highAmount = highpassMix.next();
    if(highAmount != 0.f)
      input += highAmount * (high - input);
    const float low = lowpass[1].process(lowpass[0].process(input));
    const float lowAmount = lowpassMix.next();
    return lowAmount == 0.f ? input : input + lowAmount * (low - input);
  }
};

struct BandFilter
{
  Lowpass begin, end;
  LinearRamp beginMix, endMix, activity;
  BandParameters parameters;
  double rate = 96000.;
  int rampSamples = 480;

  static float active(const BandParameters& p, double rate = 96000.) noexcept
  {
    const bool empty = p.begin_enabled && p.end_enabled
        && Lowpass::effectiveFrequency(p.begin_hz, rate)
               >= Lowpass::effectiveFrequency(p.end_hz, rate);
    return p.enabled && !empty ? 1.f : 0.f;
  }
  void prepare(const BandParameters& p, double sampleRate) noexcept
  {
    parameters = p;
    rate = sampleRate;
    rampSamples = std::max(1, int(rate * 0.005));
    begin.prepare(p.begin_hz, rate);
    end.prepare(p.end_hz, rate);
    beginMix.reset(p.begin_enabled ? 1.f : 0.f);
    endMix.reset(p.end_enabled ? 1.f : 0.f);
    activity.reset(active(p, rate));
  }
  void setParameters(const BandParameters& p) noexcept
  {
    if(p.begin_hz != parameters.begin_hz)
      begin.setFrequency(p.begin_hz, rate, rampSamples);
    if(p.end_hz != parameters.end_hz)
      end.setFrequency(p.end_hz, rate, rampSamples);
    beginMix.setTarget(p.begin_enabled ? 1.f : 0.f, rampSamples);
    endMix.setTarget(p.end_enabled ? 1.f : 0.f, rampSamples);
    activity.setTarget(active(p, rate), rampSamples);
    parameters = p;
  }
  float process(float input) noexcept
  {
    // All six endpoints run even when bypassed, empty, or disabled. Equal
    // endpoints therefore have equal histories and coefficient trajectories.
    const float low = begin.process(input);
    const float high = end.process(input);
    const float lower = low * beginMix.next();
    const float upper = input + endMix.next() * (high - input);
    activity.next();
    return upper - lower;
  }
};

struct Compressor
{
  float gain = 1.f;
  float attack = 0.f;
  float release = 0.f;
  float thresholdDb = -18.f;
  float threshold = dbToGain(-18.f);
  float slope = 0.75f;
  bool enabled = false;
  LinearRamp wet;
  float appliedGain = 1.f;
  int rampSamples = 480;
  bool configured = false;

  static float timeCoefficient(float milliseconds, double sampleRate) noexcept
  {
    return float(std::exp(-1000. / (double(milliseconds) * sampleRate)));
  }
  void reset() noexcept
  {
    gain = appliedGain = 1.f;
    wet.reset(enabled ? 1.f : 0.f);
  }
  void configure(const BandParameters& p, double rate) noexcept
  {
    enabled = p.dynamics_enabled && p.ratio > 1.f;
    attack = timeCoefficient(p.attack_ms, rate);
    release = timeCoefficient(p.release_ms, rate);
    thresholdDb = p.threshold_db;
    threshold = dbToGain(thresholdDb);
    slope = 1.f - 1.f / p.ratio;
    rampSamples = std::max(1, int(rate * 0.005));
    if(configured)
      wet.setTarget(enabled ? 1.f : 0.f, rampSamples);
    else
      wet.reset(enabled ? 1.f : 0.f);
    configured = true;
  }
  float process(float input) noexcept
  {
    const float amount = wet.next();
    if(amount == 0.f && wet.target == 0.f)
    {
      gain = appliedGain = 1.f;
      return input;
    }
    if(!finite(gain))
      gain = 1.f;
    const float magnitude = std::abs(input);
    float desired = 1.f;
    if(magnitude > threshold)
    {
      const float levelDb = 20.f * std::log10(magnitude);
      desired = dbToGain(-(levelDb - thresholdDb) * slope);
    }
    // No independently smoothed detector: these are the only A/R constants.
    const float coefficient = desired < gain ? attack : release;
    gain = desired + coefficient * (gain - desired);
    appliedGain = 1.f + amount * (gain - 1.f);
    return input * appliedGain;
  }
  float gainReduction() const noexcept
  {
    return appliedGain > 0.f && appliedGain < 1.f ? -20.f * std::log10(appliedGain) : 0.f;
  }
};

struct DcBlocker
{
  float previousInput = 0.f;
  float previousOutput = 0.f;
  float pole = 0.f;
  float normalization = 0.5f;

  void reset() noexcept { previousInput = previousOutput = 0.f; }
  void prepare(double rate, float frequency = 5.f) noexcept
  {
    pole = float(std::exp(-2. * std::numbers::pi * frequency / rate));
    normalization = 0.5f * (1.f + pole);
    reset();
  }
  float process(float input) noexcept
  {
    if(!finite(previousInput) || !finite(previousOutput))
      reset();
    input = audioSample(input, 1.e6f);
    const float output = normalization * (input - previousInput) + pole * previousOutput;
    previousInput = input;
    previousOutput = flushSmall(audioSample(output, 1.e6f));
    return previousOutput;
  }
};

inline float boundedSend(float value) noexcept
{
  value = audioSample(value, 1.e6f);
  return value / (1.f + std::abs(value));
}

struct FeedbackMatrix
{
  std::array<LinearRamp, 9> routes;
  LinearRamp enable;
  std::array<float, 3> previous{}, older{};
  int rampSamples = 480;

  void clear() noexcept
  {
    previous.fill(0.f);
    older.fill(0.f);
  }
  void prepare(double rate, const std::array<float, 9>& matrix, bool enabled) noexcept
  {
    rampSamples = std::max(1, int(rate * 0.005));
    for(int i = 0; i < 9; ++i)
      routes[i].reset(sanitize(matrix[i], -1.f, 1.f, 0.f));
    enable.reset(enabled ? 1.f : 0.f);
    clear();
  }
  void setParameters(const std::array<float, 9>& matrix, bool enabled) noexcept
  {
    for(int i = 0; i < 9; ++i)
      routes[i].setTarget(sanitize(matrix[i], -1.f, 1.f, 0.f), rampSamples);
    enable.setTarget(enabled ? 1.f : 0.f, rampSamples);
  }
  bool active() const noexcept { return enable.value > 0.f; }
  std::array<float, 3> returns() noexcept
  {
    const bool wasActive = active();
    const float amount = enable.next();
    if(amount == 0.f && enable.target == 0.f)
    {
      if(wasActive)
        clear();
      // Keep route automation current without computing disabled returns.
      for(auto& route : routes)
        route.next();
      return {};
    }
    std::array<float, 3> snapshot{}, result{};
    for(int src = 0; src < 3; ++src)
      snapshot[src] = 0.5f * (audioSample(previous[src], 1.f) + audioSample(older[src], 1.f));
    for(int dst = 0; dst < 3; ++dst)
      for(int src = 0; src < 3; ++src)
        result[dst] += routes[dst * 3 + src].next() * snapshot[src];
    for(auto& value : result)
      value = audioSample(value, 3.f) * amount;
    return result;
  }
  void commit(const std::array<float, 3>& sends) noexcept
  {
    if(!active())
      return;
    older = previous;
    for(int src = 0; src < 3; ++src)
      previous[src] = audioSample(sends[src], 1.f);
  }
};

class FadingShaper
{
  enum class Phase { steady, fadingOut, fadingIn };
  Shaper shaper;
  LinearRamp envelope;
  Phase phase = Phase::steady;
  int current = 0;
  int requested = 0;
  int fadeSamples = 192;

public:
  void prepare(double rate, int algorithm) noexcept
  {
    fadeSamples = std::max(1, int(rate * 0.002));
    current = requested = algorithm;
    shaper.reset(current);
    envelope.reset(1.f);
    phase = Phase::steady;
  }
  void request(int algorithm) noexcept
  {
    requested = algorithm >= 0 && algorithm < algorithm_count ? algorithm : 0;
    if(phase != Phase::fadingOut && requested != current)
    {
      envelope.setTarget(0.f, fadeSamples);
      phase = Phase::fadingOut;
    }
  }
  float process(float input, float drive, bool active = true) noexcept
  {
    const float amount = envelope.next();
    if(phase == Phase::fadingOut && envelope.remaining == 0)
    {
      // Only the last requested algorithm is installed, at exactly zero gain.
      current = requested;
      shaper.reset(current);
      envelope.setTarget(1.f, fadeSamples);
      phase = Phase::fadingIn;
      return 0.f;
    }
    if(phase == Phase::fadingIn && envelope.remaining == 0)
      phase = Phase::steady;
    if(!active)
      return 0.f;
    input = audioSample(input);
    const float output = current == 0 ? input : shaper.process(input, drive);
    if(!finite(output))
    {
      shaper.reset(current);
      return 0.f;
    }
    return audioSample(output, 1.e6f) * amount;
  }
  int algorithm() const noexcept { return current; }
  float fadeGain() const noexcept { return envelope.value; }
};

class Engine
{
  struct Band
  {
    BandFilter filter;
    FilterPair preFilters, postFilters;
    Compressor compressor;
    FadingShaper shaper;
    DcBlocker sendDc;
    LinearRamp drive, level;

    void prepare(double rate, const BandParameters& p) noexcept
    {
      filter.prepare(p, rate);
      preFilters.prepare(p.pre_filters, rate);
      postFilters.prepare(p.post_filters, rate);
      compressor.configure(p, rate);
      compressor.reset();
      shaper.prepare(rate, p.algorithm);
      sendDc.prepare(rate, 10.f);
      drive.reset(dbToGain(p.drive_db));
      level.reset(dbToGain(p.level_db));
    }
    void setParameters(double rate, const BandParameters& p) noexcept
    {
      const auto& old = filter.parameters;
      if(p.drive_db != old.drive_db)
        drive.setTarget(dbToGain(p.drive_db), filter.rampSamples);
      if(p.level_db != old.level_db)
        level.setTarget(dbToGain(p.level_db), filter.rampSamples);
      if(p.dynamics_enabled != old.dynamics_enabled || p.attack_ms != old.attack_ms
         || p.release_ms != old.release_ms || p.threshold_db != old.threshold_db
         || p.ratio != old.ratio)
        compressor.configure(p, rate);
      shaper.request(p.algorithm);
      preFilters.setParameters(p.pre_filters);
      postFilters.setParameters(p.post_filters);
      filter.setParameters(p);
    }
  };

  Parameters parameters;
  std::array<Band, 3> bands;
  FeedbackMatrix feedback;
  halfband::Interpolator2x interpolator;
  halfband::Decimator2x decimator;
  DcBlocker outputDc;
  LinearRamp inputGain, outputGain;
  FilterPair inputFilters;
  double internalRate = 96000.;
  bool prepared = false;

  float processInternal(float input) noexcept
  {
    input = audioSample(input);
    input = inputFilters.process(audioSample(input * inputGain.next()));
    std::array<float, 3> extracted{}, sends{};
    for(int i = 0; i < 3; ++i)
      extracted[i] = bands[i].filter.process(input);
    const auto returns = feedback.returns();
    float sum = 0.f;
    for(int i = 0; i < 3; ++i)
    {
      auto& band = bands[i];
      const float activity = band.filter.activity.value;
      const float drive = band.drive.next();
      const float level = band.level.next();
      const bool active = activity > 0.f;
      // Pre-filtering includes delayed returns and keeps running when muted.
      const float combined = band.preFilters.process(audioSample(extracted[i] + returns[i]));
      float shaped = 0.f;
      if(active)
      {
        // Detection follows pre-filtering of extraction and matrix returns.
        shaped = band.shaper.process(band.compressor.process(combined), drive);
      }
      else
      {
        // Still advance pending shape transitions while inaudible.
        band.shaper.process(0.f, drive, false);
        band.compressor.reset();
      }
      shaped = band.postFilters.process(shaped);
      if(active && feedback.active())
        sends[i] = boundedSend(band.sendDc.process(shaped)) * activity;
      else
        band.sendDc.reset();
      sum += shaped * activity * level;
    }
    // A whole-sample snapshot/commit boundary; iteration order cannot expose
    // any current-sample source to a later destination.
    feedback.commit(sends);
    return outputDc.process(sum) * outputGain.next();
  }

public:
  void prepare(double sampleRate) noexcept
  {
    sampleRate = finite(sampleRate) ? std::clamp(sampleRate, 1000., 768000.) : 48000.;
    internalRate = sampleRate * 2.;
    // Re-prepare retains controls but resets every rate-dependent history.
    for(int i = 0; i < 3; ++i)
      bands[i].prepare(internalRate, parameters.bands[i]);
    feedback.prepare(internalRate, parameters.matrix, parameters.feedback_enabled);
    interpolator.prepare();
    decimator.prepare();
    outputDc.prepare(internalRate);
    inputGain.reset(dbToGain(parameters.input_db));
    inputFilters.prepare(parameters.input_filters, internalRate);
    outputGain.reset(dbToGain(parameters.output_db));
    prepared = true;
  }
  void setParameters(const Parameters& requested) noexcept
  {
    const auto next = sanitize(requested);
    if(prepared)
    {
      for(int i = 0; i < 3; ++i)
        bands[i].setParameters(internalRate, next.bands[i]);
      feedback.setParameters(next.matrix, next.feedback_enabled);
      inputFilters.setParameters(next.input_filters);
      if(next.input_db != parameters.input_db)
        inputGain.setTarget(dbToGain(next.input_db), std::max(1, int(internalRate * 0.005)));
      if(next.output_db != parameters.output_db)
        outputGain.setTarget(dbToGain(next.output_db), std::max(1, int(internalRate * 0.005)));
    }
    parameters = next;
  }
  float process(float input) noexcept
  {
    if(!prepared)
      return 0.f;
    float first, second;
    interpolator.process(audioSample(input), first, second);
    first = processInternal(first);
    second = processInternal(second);
    const float output = decimator.process(first, second);
    if(!finite(output))
    {
      interpolator.reset();
      decimator.reset();
      return 0.f;
    }
    return audioSample(output, 1.e6f);
  }
  float gainReduction(int band) const noexcept
  {
    return band >= 0 && band < 3 ? bands[band].compressor.gainReduction() : 0.f;
  }
  float feedbackSample(int source, int delay) const noexcept
  {
    if(source < 0 || source >= 3)
      return 0.f;
    return delay == 1 ? feedback.previous[source] : delay == 2 ? feedback.older[source] : 0.f;
  }
};
}
