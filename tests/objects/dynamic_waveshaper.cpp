#include <catch2/catch_all.hpp>
#include <examples/Advanced/Audio/DynamicWaveshaper/Dsp.hpp>
#include <examples/Advanced/Audio/DynamicWaveshaper.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/prepare.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <numbers>
#include <vector>

namespace
{
std::atomic<bool> counting{false};
std::atomic<long> allocations{0};
void* allocate(std::size_t n)
{
  if(counting.load(std::memory_order_relaxed))
    allocations.fetch_add(1, std::memory_order_relaxed);
  if(void* p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc{};
}
}
void* operator new(std::size_t n) { return allocate(n); }
void* operator new[](std::size_t n) { return allocate(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
  try { return allocate(n); } catch(...) { return nullptr; }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
  try { return allocate(n); } catch(...) { return nullptr; }
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace
{
namespace dw = ao::dynamic_waveshaper;
constexpr double sampleRate = 48000.;

float stimulus(int sample)
{
  return 0.71f * std::sin(float(sample) * 0.071f)
         + 0.39f * std::sin(float(sample) * 0.193f);
}

dw::BandParameters fullBand()
{
  dw::BandParameters band;
  band.enabled = true;
  band.begin_enabled = false;
  band.end_enabled = false;
  band.dynamics_enabled = false;
  band.algorithm = 0;
  band.drive_db = 0.f;
  band.level_db = 0.f;
  return band;
}

dw::Parameters singleBand(int algorithm = 0)
{
  dw::Parameters parameters;
  for(auto& band : parameters.bands)
  {
    band = fullBand();
    band.enabled = false;
  }
  parameters.bands[0].enabled = true;
  parameters.bands[0].algorithm = algorithm;
  parameters.matrix.fill(0.f);
  parameters.feedback_enabled = false;
  parameters.output_db = 0.f;
  return parameters;
}

void prepare(dw::Engine& engine, const dw::Parameters& parameters)
{
  engine.setParameters(parameters);
  engine.prepare(sampleRate);
}

float effective(dw::BandFilter& filter, float input)
{
  const float extracted = filter.process(input);
  return extracted * filter.activity.value;
}

double energy(const std::vector<float>& samples)
{
  double result = 0.;
  for(float sample : samples)
    result += double(sample) * sample;
  return result;
}

std::vector<float> renderPartitioned(const std::vector<int>& blocks)
{
  auto parameters = singleBand(1);
  parameters.bands[0].drive_db = 9.f;
  parameters.bands[0].end_enabled = true;
  parameters.bands[0].end_hz = 4200.f;
  parameters.bands[0].dynamics_enabled = true;
  parameters.bands[0].threshold_db = -15.f;
  parameters.bands[0].ratio = 3.f;
  parameters.feedback_enabled = true;
  parameters.matrix[0] = 0.31f;
  parameters.input_db = 3.f;
  parameters.input_filters.highpass_enabled = true;
  parameters.input_filters.highpass_hz = 80.f;
  parameters.bands[0].pre_filters.lowpass_enabled = true;
  parameters.bands[0].pre_filters.lowpass_hz = 7000.f;
  parameters.bands[0].post_filters.highpass_enabled = true;
  parameters.bands[0].post_filters.highpass_hz = 120.f;
  dw::Engine engine;
  prepare(engine, parameters);
  constexpr int count = 4096;
  constexpr std::array events{511, 1237, 2503, count};
  std::vector<float> output(count);
  int position = 0;
  std::size_t block = 0;
  std::size_t event = 0;
  while(position < count)
  {
    if(position == events[event])
    {
      if(event == 0)
      {
        parameters.bands[0].drive_db = 24.f;
        parameters.bands[0].end_hz = 900.f;
        parameters.matrix[0] = -0.6f;
        parameters.input_db = 15.f;
        parameters.input_filters.lowpass_enabled = true;
        parameters.input_filters.lowpass_hz = 3200.f;
        parameters.bands[0].pre_filters.lowpass_hz = 1700.f;
        parameters.bands[0].post_filters.highpass_hz = 600.f;
      }
      else if(event == 1)
      {
        parameters.bands[0].algorithm = 2;
        parameters.bands[0].level_db = -7.f;
        parameters.output_db = 3.f;
        parameters.bands[0].threshold_db = -24.f;
        parameters.input_filters.highpass_hz = 300.f;
        parameters.bands[0].pre_filters.highpass_enabled = true;
        parameters.bands[0].pre_filters.highpass_hz = 230.f;
        parameters.bands[0].post_filters.lowpass_enabled = true;
        parameters.bands[0].post_filters.lowpass_hz = 2000.f;
      }
      else
      {
        parameters.feedback_enabled = false;
        parameters.bands[0].end_enabled = false;
        parameters.bands[0].dynamics_enabled = false;
        parameters.input_db = -6.f;
        parameters.input_filters.highpass_enabled = false;
        parameters.bands[0].pre_filters.lowpass_enabled = false;
        parameters.bands[0].post_filters.highpass_enabled = false;
      }
      ++event;
    }
    // Host adapters snapshot controls at every block, not only on edits.
    engine.setParameters(parameters);
    const int end = std::min({count, events[event],
                              position + blocks[block++ % blocks.size()]});
    for(; position < end; ++position)
      output[position] = engine.process(stimulus(position));
  }
  return output;
}
}

TEST_CASE("Dynamic waveshaper complementary bands reconstruct during automation",
          "[dynamic_waveshaper][bands]")
{
  for(double rate : {32000., 48000., 96000.})
  {
    CAPTURE(rate);
    std::array<dw::BandParameters, 3> parameters;
    for(auto& band : parameters)
      band = fullBand();
    parameters[0].end_enabled = true;
    parameters[0].end_hz = 400.f;
    parameters[1].begin_enabled = parameters[1].end_enabled = true;
    parameters[1].begin_hz = 400.f;
    parameters[1].end_hz = 3000.f;
    parameters[2].begin_enabled = true;
    parameters[2].begin_hz = 3000.f;
    std::array<dw::BandFilter, 3> filters;
    for(int band = 0; band < 3; ++band)
      filters[band].prepare(parameters[band], rate);
    for(int sample = 0; sample < 6000; ++sample)
    {
      if(sample == 137 || sample == 211 || sample == 2501)
      {
        const float low = sample == 137 ? 900.f : 180.f;
        const float high = sample == 2501 ? 20000.f : 6200.f;
        parameters[0].end_hz = parameters[1].begin_hz = low;
        parameters[1].end_hz = parameters[2].begin_hz = high;
        for(int band = 0; band < 3; ++band)
          filters[band].setParameters(parameters[band]);
      }
      const float input = stimulus(sample);
      float sum = 0.f;
      for(auto& filter : filters)
        sum += effective(filter, input);
      REQUIRE(sum == Catch::Approx(input).margin(5.e-7));
    }
  }
}

TEST_CASE("Dynamic waveshaper optional endpoints have the specified frequency response",
          "[dynamic_waveshaper][bands]")
{
  constexpr float frequency = 1500.f;
  constexpr int period = 32;
  constexpr int measured = period * 128;
  for(bool lower : {false, true})
    for(bool upper : {false, true})
    {
      CAPTURE(lower, upper);
      auto parameters = fullBand();
      parameters.begin_enabled = lower;
      parameters.end_enabled = upper;
      parameters.begin_hz = 600.f;
      parameters.end_hz = 4000.f;
      dw::BandFilter filter;
      filter.prepare(parameters, sampleRate);
      double sine = 0., cosine = 0.;
      for(int sample = 0; sample < 4096 + measured; ++sample)
      {
        const double phase = 2. * std::numbers::pi * sample / period;
        const float output = effective(filter, float(std::sin(phase)));
        if(sample >= 4096)
        {
          sine += output * std::sin(phase);
          cosine += output * std::cos(phase);
        }
      }
      // H_lp = 1 / (1 + j tan(w/2) / tan(wc/2)).
      const auto response = [](float cutoff) {
        const double ratio = std::tan(std::numbers::pi * frequency / sampleRate)
                             / std::tan(std::numbers::pi * cutoff / sampleRate);
        return std::array{1. / (1. + ratio * ratio),
                          -ratio / (1. + ratio * ratio)};
      };
      const auto low = lower ? response(parameters.begin_hz) : std::array{0., 0.};
      const auto high = upper ? response(parameters.end_hz) : std::array{1., 0.};
      REQUIRE(2. * sine / measured == Catch::Approx(high[0] - low[0]).margin(2.e-5));
      REQUIRE(2. * cosine / measured == Catch::Approx(high[1] - low[1]).margin(2.e-5));
    }
}

TEST_CASE("Dynamic waveshaper invalid and disabled bands fade out and recover",
          "[dynamic_waveshaper][bands]")
{
  for(bool reversed : {false, true})
  {
    CAPTURE(reversed);
    auto parameters = fullBand();
    parameters.begin_enabled = parameters.end_enabled = true;
    parameters.begin_hz = 5000.f;
    parameters.end_hz = reversed ? 1000.f : 5000.f;
    dw::BandFilter filter;
    filter.prepare(parameters, sampleRate);
    for(int sample = 0; sample < 500; ++sample)
      REQUIRE(effective(filter, stimulus(sample)) == 0.f);

    parameters.begin_enabled = parameters.end_enabled = false;
    filter.setParameters(parameters);
    for(int sample = 0; sample < 240; ++sample)
      effective(filter, stimulus(sample));
    REQUIRE(effective(filter, 0.7f) == Catch::Approx(0.7f).margin(1.e-6));

    parameters.enabled = false;
    filter.setParameters(parameters);
    float previous = 1.f;
    for(int sample = 0; sample < 240; ++sample)
    {
      const float output = effective(filter, 1.f);
      REQUIRE(output <= previous);
      REQUIRE(output >= 0.f);
      previous = output;
    }
    REQUIRE(effective(filter, 1.f) == 0.f);
    parameters.enabled = true;
    filter.setParameters(parameters);
    previous = 0.f;
    for(int sample = 0; sample < 240; ++sample)
    {
      const float output = effective(filter, 1.f);
      REQUIRE(output >= previous);
      REQUIRE(output <= 1.f);
      previous = output;
    }
    REQUIRE(effective(filter, 1.f) == 1.f);
  }
}

TEST_CASE("Dynamic waveshaper sample-rate-clamped empty bands cannot feed back",
          "[dynamic_waveshaper][bands][feedback]")
{
  auto parameters = singleBand(1);
  parameters.bands[0].begin_enabled = parameters.bands[0].end_enabled = true;
  parameters.bands[0].begin_hz = 10000.f;
  parameters.bands[0].end_hz = 15000.f;
  parameters.feedback_enabled = true;
  parameters.matrix[0] = 1.f;
  dw::Engine engine;
  engine.setParameters(parameters);
  engine.prepare(8000.);
  // Saved bounds are ordered, but both effective endpoints equal 3600 Hz.
  for(int sample = 0; sample < 4096; ++sample)
  {
    REQUIRE(engine.process(stimulus(sample)) == 0.f);
    REQUIRE(engine.feedbackSample(0, 1) == 0.f);
  }
  // A rate change must restore the saved 10–15 kHz band, not keep it muted.
  engine.prepare(48000.);
  double energy = 0.;
  for(int sample = 0; sample < 4096; ++sample)
  {
    const float output = engine.process(0.5f * std::sin(sample * std::numbers::pi_v<float> / 2.f));
    energy += double(output) * output;
  }
  REQUIRE(energy > 1.);
}

TEST_CASE("Dynamic waveshaper compressor follows the dB ratio law",
          "[dynamic_waveshaper][dynamics]")
{
  for(float ratio : {2.f, 4.f, 20.f})
    for(float level : {-30.f, -18.f, -12.f, -6.f, 0.f})
    {
      CAPTURE(ratio, level);
      auto parameters = fullBand();
      parameters.dynamics_enabled = true;
      parameters.threshold_db = -18.f;
      parameters.ratio = ratio;
      parameters.attack_ms = 0.1f;
      dw::Compressor compressor;
      compressor.configure(parameters, sampleRate);
      compressor.reset();
      const float input = dw::dbToGain(level);
      float output = 0.f;
      for(int sample = 0; sample < 4096; ++sample)
        output = compressor.process(input);
      const float expectedDb = level <= -18.f ? level : -18.f + (level + 18.f) / ratio;
      REQUIRE(20.f * std::log10(output) == Catch::Approx(expectedDb).margin(2.e-4));
      REQUIRE(compressor.gainReduction()
              == Catch::Approx(level - expectedDb).margin(2.e-4));
      REQUIRE(compressor.process(-input) == Catch::Approx(-output).margin(1.e-6));
    }
}

TEST_CASE("Dynamic waveshaper compressor has one attack and release time constant",
          "[dynamic_waveshaper][dynamics]")
{
  auto parameters = fullBand();
  parameters.dynamics_enabled = true;
  parameters.threshold_db = -20.f;
  parameters.ratio = 4.f;
  parameters.attack_ms = 10.f;
  parameters.release_ms = 100.f;
  dw::Compressor compressor;
  compressor.configure(parameters, sampleRate);
  compressor.reset();
  const double desired = std::pow(10., -15. / 20.);
  float output = 0.f;
  for(int sample = 0; sample < 480; ++sample)
    output = compressor.process(1.f);
  REQUIRE(output == Catch::Approx(desired + (1. - desired) / std::exp(1.)).margin(2.e-5));
  const float reducedGain = output;
  for(int sample = 0; sample < 4800; ++sample)
    output = compressor.process(0.01f);
  REQUIRE(output / 0.01f
          == Catch::Approx(1. + (reducedGain - 1.) / std::exp(1.)).margin(2.e-4));

  SECTION("Disabling dynamics fades previously reduced gain to bypass")
  {
    parameters.dynamics_enabled = false;
    compressor.configure(parameters, sampleRate);
  }
  SECTION("Ratio one fades previously reduced gain to bypass")
  {
    parameters.ratio = 1.f;
    compressor.configure(parameters, sampleRate);
  }
  float previousGain = output / 0.01f;
  for(int sample = 0; sample < 240; ++sample)
  {
    const float currentGain = compressor.process(0.01f) / 0.01f;
    REQUIRE(currentGain >= previousGain - 1.e-6f);
    REQUIRE(currentGain <= 1.f + 1.e-6f);
    if(sample == 0)
      REQUIRE(currentGain < 0.999f);
    previousGain = currentGain;
  }
  for(int sample = 0; sample < 100; ++sample)
  {
    const float input = stimulus(sample);
    REQUIRE(compressor.process(input) == input);
  }
  REQUIRE(compressor.gainReduction() == 0.f);
}

TEST_CASE("Dynamic waveshaper every signed matrix route uses destination rows",
          "[dynamic_waveshaper][feedback]")
{
  constexpr std::array<float, 3> first{0.2f, -0.4f, 0.8f};
  constexpr std::array<float, 3> second{-0.6f, 0.1f, 0.3f};
  for(int destination = 0; destination < 3; ++destination)
    for(int source = 0; source < 3; ++source)
      for(float sign : {-1.f, 1.f})
      {
        CAPTURE(destination, source, sign);
        std::array<float, 9> routes{};
        routes[destination * 3 + source] = sign;
        dw::FeedbackMatrix matrix;
        matrix.prepare(sampleRate, routes, true);
        for(float value : matrix.returns())
          REQUIRE(value == 0.f);
        matrix.commit(first);
        const auto one = matrix.returns();
        matrix.commit(second);
        const auto two = matrix.returns();
        for(int band = 0; band < 3; ++band)
        {
          REQUIRE(one[band] == Catch::Approx(band == destination ? sign * first[source] * 0.5f : 0.f).margin(1.e-7));
          REQUIRE(two[band] == Catch::Approx(band == destination ? sign * (first[source] + second[source]) * 0.5f : 0.f).margin(1.e-7));
        }
      }
}

TEST_CASE("Dynamic waveshaper cycles read one simultaneous two-sample snapshot",
          "[dynamic_waveshaper][feedback]")
{
  const std::array<float, 9> routes{0.2f, -0.7f, 0.4f,
                                    0.8f, 0.1f, -0.6f,
                                    -0.3f, 0.9f, -0.5f};
  dw::FeedbackMatrix matrix;
  matrix.prepare(sampleRate, routes, true);
  std::array<float, 3> previous{}, older{};
  for(int sample = 0; sample < 200; ++sample)
  {
    const auto returns = matrix.returns();
    std::array<float, 3> next{};
    for(int destination = 2; destination >= 0; --destination)
    {
      float expected = 0.f;
      for(int source = 0; source < 3; ++source)
        expected += routes[destination * 3 + source]
                    * (previous[source] + older[source]) * 0.5f;
      REQUIRE(returns[destination] == Catch::Approx(expected).margin(2.e-7));
      next[destination] = dw::boundedSend(expected + stimulus(sample + 19 * destination));
    }
    matrix.commit(next);
    older = previous;
    previous = next;
  }
}

TEST_CASE("Dynamic waveshaper sends are bounded and feedback off clears memory",
          "[dynamic_waveshaper][feedback]")
{
  for(float value : {-1.e9f, -10.f, -1.f, -0.01f, 0.f, 0.01f, 1.f, 10.f, 1.e9f})
  {
    const float send = dw::boundedSend(value);
    REQUIRE(dw::finite(send));
    REQUIRE(std::abs(send) < 1.f);
    REQUIRE(send == Catch::Approx(-dw::boundedSend(-value)).margin(1.e-7));
    if(value != 0.f)
      REQUIRE(send * value > 0.f);
  }
  REQUIRE(dw::boundedSend(1.f) == Catch::Approx(0.5f));
  REQUIRE(dw::boundedSend(std::numeric_limits<float>::infinity()) == 0.f);
  REQUIRE(dw::boundedSend(std::numeric_limits<float>::quiet_NaN()) == 0.f);

  std::array<float, 9> routes;
  routes.fill(1.f);
  dw::FeedbackMatrix matrix;
  matrix.prepare(sampleRate, routes, true);
  matrix.commit({100.f, 100.f, 100.f});
  matrix.commit({100.f, 100.f, 100.f});
  for(float value : matrix.returns())
    REQUIRE(value == 3.f); // No hidden row normalization.
  matrix.setParameters(routes, false);
  float previous = 3.f;
  for(int sample = 0; sample < 240; ++sample)
  {
    const auto result = matrix.returns();
    REQUIRE(result[0] <= previous);
    REQUIRE(result[0] >= 0.f);
    previous = result[0];
  }
  REQUIRE(previous == 0.f);
  matrix.setParameters(routes, true);
  for(int sample = 0; sample < 240; ++sample)
    for(float value : matrix.returns())
      REQUIRE(value == 0.f);
  matrix.commit({0.4f, 0.4f, 0.4f});
  for(float value : matrix.returns())
    REQUIRE(value == Catch::Approx(0.6f).margin(1.e-6));
}

TEST_CASE("Dynamic waveshaper all 43 algorithms are quiet, active, and reset repeatably",
          "[dynamic_waveshaper][shapers]")
{
  for(int algorithm = 0; algorithm < 43; ++algorithm)
  {
    CAPTURE(algorithm, dw::algorithmName(algorithm));
    dw::Shaper shaper;
    shaper.reset(algorithm);
    for(int sample = 0; sample < 256; ++sample)
      REQUIRE(shaper.process(0.f, 1.f) == Catch::Approx(0.f).margin(2.e-6));

    double outputEnergy = 0., difference = 0.;
    for(float drive : {0.25f, 1.f, 4.f, 32.f})
    {
      CAPTURE(drive);
      shaper.reset(algorithm);
      std::array<float, 1024> first{};
      for(int sample = 0; sample < int(first.size()); ++sample)
      {
        const float input = stimulus(sample);
        const float output = first[sample] = shaper.process(input, drive);
        REQUIRE(dw::finite(output));
        outputEnergy += double(output) * output;
        difference += double(output - input) * (output - input);
        if(algorithm == 0)
          REQUIRE(output == input);
      }
      shaper.reset(algorithm);
      for(int sample = 0; sample < int(first.size()); ++sample)
        REQUIRE(shaper.process(stimulus(sample), drive)
                == Catch::Approx(first[sample]).margin(1.e-6));
    }
    // Coarse quantization can erase an individual drive/input combination,
    // but an inert catalog entry cannot pass the complete excitation sweep.
    REQUIRE(outputEnergy > 1.e-5);
    if(algorithm != 0)
      REQUIRE(difference > 1.e-4);
  }
}

TEST_CASE("Dynamic waveshaper shape switches reset histories at zero gain",
          "[dynamic_waveshaper][shapers]")
{
  constexpr int fadeSamples = 96;
  for(int target = 0; target < 43; ++target)
  {
    CAPTURE(target);
    dw::FadingShaper shaper;
    shaper.prepare(sampleRate, (target + 1) % 43);
    for(int sample = 0; sample < 500; ++sample)
      shaper.process(stimulus(sample), 3.f);
    shaper.request(target);
    float last = 1.f;
    for(int sample = 0; sample < fadeSamples; ++sample)
      last = shaper.process(stimulus(sample), 3.f);
    REQUIRE(last == 0.f);
    dw::Shaper fresh;
    fresh.reset(target);
    for(int sample = 0; sample < 400; ++sample)
    {
      const float input = stimulus(sample + 97);
      const float envelope = std::min(1.f, float(sample + 1) / fadeSamples);
      REQUIRE(shaper.process(input, 3.f)
              == Catch::Approx(fresh.process(input, 3.f) * envelope).margin(3.e-5));
    }
  }

  SECTION("Rapid requests coalesce and inactive processing still completes switching")
  {
    dw::FadingShaper shaper;
    shaper.prepare(sampleRate, 0);
    shaper.request(1);
    for(int sample = 0; sample < 20; ++sample)
      REQUIRE(shaper.process(0.8f, 2.f, false) == 0.f);
    shaper.request(2);
    for(int sample = 20; sample < 2 * fadeSamples; ++sample)
      REQUIRE(shaper.process(0.8f, 2.f, false) == 0.f);
    dw::Shaper fresh;
    fresh.reset(2);
    for(int sample = 0; sample < 300; ++sample)
      REQUIRE(shaper.process(stimulus(sample), 2.f)
              == Catch::Approx(fresh.process(stimulus(sample), 2.f)).margin(1.e-6));
  }
}

TEST_CASE("Dynamic waveshaper automation is independent of host block partition",
          "[dynamic_waveshaper][engine]")
{
  const auto reference = renderPartitioned({4096});
  REQUIRE(energy(reference) > 1.);
  for(const auto& partition : {std::vector<int>{1}, std::vector<int>{17, 64, 3, 127},
                               std::vector<int>{256, 511}})
  {
    const auto output = renderPartitioned(partition);
    REQUIRE(output.size() == reference.size());
    for(std::size_t sample = 0; sample < output.size(); ++sample)
      REQUIRE(output[sample] == Catch::Approx(reference[sample]).margin(2.e-6));
  }
}

TEST_CASE("Dynamic waveshaper engine exposes audible shaping and Off bypasses drive",
          "[dynamic_waveshaper][engine]")
{
  auto dryParameters = singleBand();
  auto drivenParameters = dryParameters;
  drivenParameters.bands[0].drive_db = 36.f;
  auto shapedParameters = drivenParameters;
  shapedParameters.bands[0].algorithm = 1;
  dw::Engine dry, driven, shaped;
  prepare(dry, dryParameters);
  prepare(driven, drivenParameters);
  prepare(shaped, shapedParameters);
  double difference = 0., shapedEnergy = 0.;
  for(int sample = 0; sample < 4096; ++sample)
  {
    const float input = stimulus(sample);
    const float reference = dry.process(input);
    REQUIRE(driven.process(input) == Catch::Approx(reference).margin(1.e-7));
    const float output = shaped.process(input);
    difference += double(output - reference) * (output - reference);
    shapedEnergy += double(output) * output;
  }
  REQUIRE(shapedEnergy > 1.);
  REQUIRE(difference > 1.);
}

TEST_CASE("Dynamic waveshaper overlapping full-range bands add without a dry residual",
          "[dynamic_waveshaper][engine][bands]")
{
  auto oneParameters = singleBand();
  auto threeParameters = oneParameters;
  for(auto& band : threeParameters.bands)
    band.enabled = true;
  auto emptyParameters = oneParameters;
  emptyParameters.bands[0].enabled = false;
  dw::Engine one, three, empty;
  prepare(one, oneParameters);
  prepare(three, threeParameters);
  prepare(empty, emptyParameters);
  double audible = 0.;
  for(int sample = 0; sample < 4096; ++sample)
  {
    const float input = stimulus(sample);
    const float output = one.process(input);
    audible += double(output) * output;
    REQUIRE(three.process(input) == Catch::Approx(3.f * output).margin(5.e-5));
    REQUIRE(empty.process(input) == 0.f);
  }
  REQUIRE(audible > 1.);
}

TEST_CASE("Dynamic waveshaper disabled and invalid bands remove output and sends",
          "[dynamic_waveshaper][engine][feedback]")
{
  for(bool invalid : {false, true})
  {
    CAPTURE(invalid);
    auto parameters = singleBand(1);
    parameters.feedback_enabled = true;
    parameters.matrix[0] = 0.8f;
    dw::Engine engine;
    prepare(engine, parameters);
    double sendEnergy = 0.;
    for(int sample = 0; sample < 2000; ++sample)
    {
      engine.process(stimulus(sample));
      sendEnergy += std::abs(engine.feedbackSample(0, 1));
    }
    REQUIRE(sendEnergy > 1.);
    if(invalid)
    {
      parameters.bands[0].begin_enabled = parameters.bands[0].end_enabled = true;
      parameters.bands[0].begin_hz = 5000.f;
      parameters.bands[0].end_hz = 500.f;
    }
    else
      parameters.bands[0].enabled = false;
    engine.setParameters(parameters);
    float output = 0.f;
    for(int sample = 0; sample < 24000; ++sample)
    {
      output = engine.process(stimulus(sample));
      if(sample >= 500)
      {
        REQUIRE(engine.feedbackSample(0, 1) == 0.f);
        REQUIRE(engine.feedbackSample(0, 2) == 0.f);
      }
    }
    // The global DC blocker has a tail; muting must not leave a dry signal.
    REQUIRE(std::abs(output) < 2.e-5f);
  }
}

TEST_CASE("Dynamic waveshaper sends precede band Level and excite after extraction",
          "[dynamic_waveshaper][engine][feedback]")
{
  auto parameters = singleBand(1);
  parameters.bands[0].drive_db = 18.f;
  parameters.bands[1] = fullBand();
  parameters.bands[1].begin_enabled = true;
  parameters.bands[1].begin_hz = 18000.f;
  parameters.feedback_enabled = true;
  parameters.matrix[3] = 0.8f; // From band 1 to band 2.
  auto quietSource = parameters;
  quietSource.bands[0].level_db = -60.f;
  auto noRoute = quietSource;
  noRoute.matrix.fill(0.f);
  dw::Engine loud, quiet, disconnected;
  prepare(loud, parameters);
  prepare(quiet, quietSource);
  prepare(disconnected, noRoute);
  double returnDifference = 0., sourceActivity = 0.;
  for(int sample = 0; sample < 4096; ++sample)
  {
    const float input = 0.5f * std::sin(float(sample) * 0.013f);
    loud.process(input);
    const float output = quiet.process(input);
    const float unrouted = disconnected.process(input);
    for(int source = 0; source < 3; ++source)
      for(int delay : {1, 2})
        REQUIRE(quiet.feedbackSample(source, delay)
                == Catch::Approx(loud.feedbackSample(source, delay)).margin(1.e-6));
    sourceActivity += std::abs(quiet.feedbackSample(0, 1));
    returnDifference += double(output - unrouted) * (output - unrouted);
  }
  REQUIRE(sourceActivity > 1.);
  REQUIRE(returnDifference > 10.);
}

TEST_CASE("Dynamic waveshaper dynamics detects delayed returns and publishes reduction",
          "[dynamic_waveshaper][engine][dynamics]")
{
  auto parameters = singleBand(1);
  parameters.bands[0].drive_db = 24.f;
  parameters.bands[0].level_db = -60.f;
  parameters.bands[1] = fullBand();
  parameters.bands[1].begin_enabled = true;
  parameters.bands[1].begin_hz = 18000.f;
  parameters.bands[1].dynamics_enabled = true;
  parameters.bands[1].threshold_db = -18.f;
  parameters.bands[1].ratio = 8.f;
  parameters.bands[1].attack_ms = 0.1f;
  parameters.bands[1].release_ms = 100.f;
  parameters.feedback_enabled = true;
  parameters.matrix[3] = 1.f;
  auto disconnectedParameters = parameters;
  disconnectedParameters.matrix.fill(0.f);
  dw::Engine connected, disconnected;
  prepare(connected, parameters);
  prepare(disconnected, disconnectedParameters);
  for(int sample = 0; sample < 6000; ++sample)
  {
    const float input = 0.5f * std::sin(float(sample) * 0.013f);
    connected.process(input);
    disconnected.process(input);
  }
  REQUIRE(connected.gainReduction(1) > 3.f);
  REQUIRE(disconnected.gainReduction(1) == Catch::Approx(0.f).margin(1.e-6));
}

TEST_CASE("Dynamic waveshaper all algorithms initialize quietly in the engine",
          "[dynamic_waveshaper][engine][shapers]")
{
  for(int algorithm = 0; algorithm < 43; ++algorithm)
  {
    CAPTURE(algorithm, dw::algorithmName(algorithm));
    auto parameters = singleBand(algorithm);
    parameters.feedback_enabled = true;
    parameters.matrix[0] = 0.7f;
    dw::Engine engine;
    prepare(engine, parameters);
    for(int sample = 0; sample < 512; ++sample)
    {
      REQUIRE(engine.process(0.f) == Catch::Approx(0.f).margin(3.e-6));
      REQUIRE(engine.feedbackSample(0, 1) == Catch::Approx(0.f).margin(3.e-6));
    }
    double activity = 0.;
    for(int sample = 0; sample < 1024; ++sample)
    {
      const float output = engine.process(stimulus(sample));
      REQUIRE(dw::finite(output));
      activity += double(output) * output;
    }
    REQUIRE(activity > 1.e-5);
    engine.prepare(sampleRate);
    for(int sample = 0; sample < 512; ++sample)
      REQUIRE(engine.process(0.f) == Catch::Approx(0.f).margin(3.e-6));
  }
}

TEST_CASE("Dynamic waveshaper high-drive cycles remain bounded and recover after clearing",
          "[dynamic_waveshaper][engine][feedback]")
{
  // Exercising the full catalog includes Digital, rectifiers, and every Fuzz.
  for(int algorithm = 0; algorithm < 43; ++algorithm)
  {
    CAPTURE(algorithm, dw::algorithmName(algorithm));
    auto parameters = singleBand(algorithm);
    for(auto& band : parameters.bands)
    {
      band.enabled = true;
      band.algorithm = algorithm;
      band.drive_db = 36.f;
    }
    parameters.feedback_enabled = true;
    parameters.matrix = {1.f, -1.f, 1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f};
    dw::Engine engine;
    prepare(engine, parameters);
    double outputEnergy = 0., sendEnergy = 0.;
    for(int sample = 0; sample < 3000; ++sample)
    {
      float input = sample < 1500 ? 32.f * stimulus(sample) : 0.f;
      if(sample == 101)
        input = std::numeric_limits<float>::quiet_NaN();
      else if(sample == 205)
        input = std::numeric_limits<float>::infinity();
      const float output = engine.process(input);
      REQUIRE(dw::finite(output));
      REQUIRE(std::abs(output) <= 1.e6f);
      outputEnergy += double(output) * output;
      for(int source = 0; source < 3; ++source)
        for(int delay : {1, 2})
        {
          const float send = engine.feedbackSample(source, delay);
          REQUIRE(dw::finite(send));
          REQUIRE(std::abs(send) <= 1.f);
          sendEnergy += std::abs(send);
        }
    }
    REQUIRE(outputEnergy > 1.e-5);
    REQUIRE(sendEnergy > 1.e-5);
    parameters.feedback_enabled = false;
    engine.setParameters(parameters);
    for(int sample = 0; sample < 512; ++sample)
      engine.process(0.f);
    for(int source = 0; source < 3; ++source)
      for(int delay : {1, 2})
        REQUIRE(engine.feedbackSample(source, delay) == 0.f);

    // Re-prepare must erase nonlinear, feedback, filter, and resampler history.
    parameters = singleBand(algorithm);
    engine.setParameters(parameters);
    engine.prepare(sampleRate);
    for(int sample = 0; sample < 256; ++sample)
      REQUIRE(engine.process(0.f) == Catch::Approx(0.f).margin(3.e-6));
    double recovered = 0.;
    for(int sample = 0; sample < 1024; ++sample)
    {
      const float output = engine.process(stimulus(sample));
      recovered += double(output) * output;
    }
    REQUIRE(recovered > 1.e-5);
  }
}

TEST_CASE("Dynamic waveshaper serial filter endpoints have non-resonant two-pole responses",
          "[dynamic_waveshaper][filters]")
{
  for(double rate : {16000., 96000., 192000.})
    for(bool highpass : {false, true})
      for(bool lowpass : {false, true})
      {
        CAPTURE(rate, highpass, lowpass);
        dw::FilterParameters parameters;
        parameters.highpass_enabled = highpass;
        parameters.lowpass_enabled = lowpass;
        parameters.highpass_hz = 600.f;
        parameters.lowpass_hz = 4000.f;
        dw::FilterPair filter;
        filter.prepare(parameters, rate);
        constexpr int period = 32, measured = period * 256;
        const double frequency = rate / period;
        double sine = 0., cosine = 0.;
        for(int sample = 0; sample < 8192 + measured; ++sample)
        {
          const double phase = 2. * std::numbers::pi * sample / period;
          const float input = float(std::sin(phase));
          const float output = filter.process(input);
          if(!highpass && !lowpass)
            REQUIRE(output == input);
          if(sample >= 8192)
          {
            sine += output * std::sin(phase);
            cosine += output * std::cos(phase);
          }
        }
        const auto ratioSquared = [&](float cutoff) {
          const double ratio = std::tan(std::numbers::pi * frequency / rate)
              / std::tan(std::numbers::pi * dw::Lowpass::effectiveFrequency(cutoff, rate) / rate);
          return ratio * ratio;
        };
        const double highRatio = ratioSquared(parameters.highpass_hz);
        const double lowRatio = ratioSquared(parameters.lowpass_hz);
        const double expected = (highpass ? highRatio / (1. + highRatio) : 1.)
                                * (lowpass ? 1. / (1. + lowRatio) : 1.);
        REQUIRE(2. * std::hypot(sine, cosine) / measured
                == Catch::Approx(expected).margin(2.e-5));
      }
}

TEST_CASE("Dynamic waveshaper filter bypass keeps warm histories and crossfades smoothly",
          "[dynamic_waveshaper][filters]")
{
  for(bool highpass : {false, true})
  {
    CAPTURE(highpass);
    dw::FilterParameters bypassed;
    bypassed.highpass_hz = bypassed.lowpass_hz = 800.f;
    auto enabled = bypassed;
    enabled.highpass_enabled = highpass;
    enabled.lowpass_enabled = !highpass;
    dw::FilterPair transitioning, wet;
    transitioning.prepare(bypassed, sampleRate);
    wet.prepare(enabled, sampleRate);
    for(int sample = 0; sample < 2000; ++sample)
    {
      const float input = stimulus(sample);
      REQUIRE(transitioning.process(input) == input);
      wet.process(input);
    }
    transitioning.setParameters(enabled);
    constexpr int ramp = 240;
    for(int sample = 0; sample < 2 * ramp; ++sample)
    {
      if(sample == ramp)
        transitioning.setParameters(bypassed);
      // Unchanged block snapshots must not prolong either fade.
      transitioning.setParameters(sample < ramp ? enabled : bypassed);
      const float input = stimulus(sample + 2000);
      const float filtered = wet.process(input);
      const float amount = sample < ramp ? float(sample + 1) / ramp
                                        : 1.f - float(sample - ramp + 1) / ramp;
      REQUIRE(transitioning.process(input)
              == Catch::Approx(input + amount * (filtered - input)).margin(5.e-6));
    }
    REQUIRE(transitioning.process(0.37f) == 0.37f);
  }
}

TEST_CASE("Dynamic waveshaper bypassed new controls preserve complementary reconstruction",
          "[dynamic_waveshaper][engine][filters]")
{
  dw::Parameters defaults;
  REQUIRE(defaults.input_db == 0.f);
  REQUIRE_FALSE(defaults.input_filters.highpass_enabled);
  REQUIRE_FALSE(defaults.input_filters.lowpass_enabled);
  auto automated = defaults;
  for(auto& band : automated.bands)
  {
    REQUIRE_FALSE(band.pre_filters.highpass_enabled);
    REQUIRE_FALSE(band.pre_filters.lowpass_enabled);
    REQUIRE_FALSE(band.post_filters.highpass_enabled);
    REQUIRE_FALSE(band.post_filters.lowpass_enabled);
    band.pre_filters.highpass_hz = 20000.f;
    band.pre_filters.lowpass_hz = 20.f;
    band.post_filters.highpass_hz = 17000.f;
    band.post_filters.lowpass_hz = 30.f;
  }
  automated.input_filters.highpass_hz = 19000.f;
  automated.input_filters.lowpass_hz = 40.f;
  dw::Engine original, changed, fullRange;
  prepare(original, defaults);
  prepare(changed, automated);
  prepare(fullRange, singleBand());
  for(int sample = 0; sample < 4096; ++sample)
  {
    if(sample == 317)
    {
      automated.input_filters.highpass_hz = 20.f;
      automated.bands[1].pre_filters.lowpass_hz = 13000.f;
      automated.bands[2].post_filters.highpass_hz = 70.f;
      changed.setParameters(automated);
    }
    const float input = stimulus(sample);
    const float reference = original.process(input);
    REQUIRE(changed.process(input) == reference);
    REQUIRE(fullRange.process(input) == Catch::Approx(reference).margin(3.e-6));
  }
}

TEST_CASE("Dynamic waveshaper input gain precedes filtering and shaping even with Off selected",
          "[dynamic_waveshaper][engine][filters]")
{
  for(int algorithm : {0, 1})
    for(float inputDb : {-24.f, 12.f, 36.f})
    {
      CAPTURE(algorithm, inputDb);
      auto parameters = singleBand(algorithm);
      parameters.input_filters.highpass_enabled = true;
      parameters.input_filters.highpass_hz = 70.f;
      parameters.input_filters.lowpass_enabled = true;
      parameters.input_filters.lowpass_hz = 2500.f;
      auto boosted = parameters;
      boosted.input_db = inputDb;
      dw::Engine gainControl, externalGain;
      prepare(gainControl, boosted);
      prepare(externalGain, parameters);
      const float gain = dw::dbToGain(inputDb);
      double outputEnergy = 0.;
      for(int sample = 0; sample < 8192; ++sample)
      {
        const float input = 0.01f * stimulus(sample);
        const float output = gainControl.process(input);
        REQUIRE(output == Catch::Approx(externalGain.process(input * gain)).margin(3.e-5));
        outputEnergy += double(output) * output;
      }
      REQUIRE(outputEnergy > 1.e-5);
    }
}

TEST_CASE("Dynamic waveshaper global and band filters attenuate their stop bands",
          "[dynamic_waveshaper][engine][filters]")
{
  for(int position = 0; position < 3; ++position)
    for(bool highpass : {false, true})
    {
      CAPTURE(position, highpass);
      auto parameters = singleBand();
      auto filteredParameters = parameters;
      auto& filters = position == 0 ? filteredParameters.input_filters
                      : position == 1 ? filteredParameters.bands[0].pre_filters
                                      : filteredParameters.bands[0].post_filters;
      filters.highpass_enabled = highpass;
      filters.lowpass_enabled = !highpass;
      filters.highpass_hz = 4000.f;
      filters.lowpass_hz = 300.f;
      dw::Engine dry, filtered;
      prepare(dry, parameters);
      prepare(filtered, filteredParameters);
      double dryEnergy = 0., filteredEnergy = 0.;
      const double frequency = highpass ? 300. : 4000.;
      for(int sample = 0; sample < 24000; ++sample)
      {
        const float input = float(0.1 * std::sin(2. * std::numbers::pi * frequency * sample / sampleRate));
        const float reference = dry.process(input);
        const float output = filtered.process(input);
        if(sample >= 12000)
        {
          dryEnergy += double(reference) * reference;
          filteredEnergy += double(output) * output;
        }
      }
      REQUIRE(dryEnergy > 1.);
      REQUIRE(filteredEnergy < dryEnergy * 0.001);
    }
}

TEST_CASE("Dynamic waveshaper pre and post filtering straddle the nonlinear shaper",
          "[dynamic_waveshaper][engine][filters]")
{
  auto preParameters = singleBand(1);
  preParameters.bands[0].drive_db = 24.f;
  preParameters.bands[0].pre_filters.lowpass_enabled = true;
  preParameters.bands[0].pre_filters.lowpass_hz = 500.f;
  auto postParameters = singleBand(1);
  postParameters.bands[0].drive_db = 24.f;
  postParameters.bands[0].post_filters = preParameters.bands[0].pre_filters;
  dw::Engine pre, post;
  prepare(pre, preParameters);
  prepare(post, postParameters);
  double preEnergy = 0., postEnergy = 0., difference = 0.;
  for(int sample = 0; sample < 24000; ++sample)
  {
    const float input = float(0.8 * std::sin(2. * std::numbers::pi * 4000. * sample / sampleRate));
    const float before = pre.process(input);
    const float after = post.process(input);
    if(sample >= 12000)
    {
      preEnergy += double(before) * before;
      postEnergy += double(after) * after;
      difference += double(before - after) * (before - after);
    }
  }
  REQUIRE(preEnergy > postEnergy * 4.);
  REQUIRE(difference > 1.);
}

TEST_CASE("Dynamic waveshaper post filters shape feedback sends before band Level",
          "[dynamic_waveshaper][engine][filters][feedback]")
{
  auto parameters = singleBand();
  parameters.feedback_enabled = true;
  parameters.matrix[3] = 0.9f; // The filtered source excites another band.
  parameters.bands[1] = fullBand();
  auto filteredParameters = parameters;
  filteredParameters.bands[0].post_filters.lowpass_enabled = true;
  filteredParameters.bands[0].post_filters.lowpass_hz = 200.f;
  auto quietParameters = filteredParameters;
  quietParameters.bands[0].level_db = -60.f;
  dw::Engine dry, filtered, quiet;
  prepare(dry, parameters);
  prepare(filtered, filteredParameters);
  prepare(quiet, quietParameters);
  double drySendEnergy = 0., filteredSendEnergy = 0., destinationDifference = 0.;
  for(int sample = 0; sample < 24000; ++sample)
  {
    const float input = float(0.2 * std::sin(2. * std::numbers::pi * 4000. * sample / sampleRate));
    dry.process(input);
    filtered.process(input);
    quiet.process(input);
    for(int source = 0; source < 3; ++source)
      REQUIRE(quiet.feedbackSample(source, 1) == filtered.feedbackSample(source, 1));
    if(sample >= 12000)
    {
      const float drySend = dry.feedbackSample(0, 1);
      const float filteredSend = filtered.feedbackSample(0, 1);
      const float difference = dry.feedbackSample(1, 1) - filtered.feedbackSample(1, 1);
      drySendEnergy += double(drySend) * drySend;
      filteredSendEnergy += double(filteredSend) * filteredSend;
      destinationDifference += double(difference) * difference;
    }
  }
  REQUIRE(drySendEnergy > 1.);
  REQUIRE(filteredSendEnergy < drySendEnergy * 0.001);
  REQUIRE(destinationDifference > 1.);
}

TEST_CASE("Dynamic waveshaper pre filters attenuate delayed returns before dynamics",
          "[dynamic_waveshaper][engine][filters][feedback]")
{
  auto parameters = singleBand(1);
  parameters.bands[0].drive_db = 24.f;
  parameters.bands[1] = fullBand();
  parameters.bands[1].begin_enabled = true;
  parameters.bands[1].begin_hz = 18000.f;
  parameters.bands[1].dynamics_enabled = true;
  parameters.bands[1].threshold_db = -18.f;
  parameters.bands[1].ratio = 8.f;
  parameters.bands[1].attack_ms = 0.1f;
  parameters.feedback_enabled = true;
  parameters.matrix[3] = 1.f;
  auto filteredParameters = parameters;
  filteredParameters.bands[1].pre_filters.highpass_enabled = true;
  filteredParameters.bands[1].pre_filters.highpass_hz = 10000.f;
  dw::Engine dry, filtered;
  prepare(dry, parameters);
  prepare(filtered, filteredParameters);
  for(int sample = 0; sample < 24000; ++sample)
  {
    const float input = float(0.5 * std::sin(2. * std::numbers::pi * 100. * sample / sampleRate));
    dry.process(input);
    filtered.process(input);
  }
  REQUIRE(dry.gainReduction(1) > 3.f);
  REQUIRE(filtered.gainReduction(1) < 0.1f);
}

TEST_CASE("Dynamic waveshaper filter limits track reprepare and reject nonfinite controls",
          "[dynamic_waveshaper][engine][filters]")
{
  auto parameters = singleBand(1);
  parameters.input_db = std::numeric_limits<float>::infinity();
  parameters.input_filters.highpass_hz = std::numeric_limits<float>::quiet_NaN();
  parameters.input_filters.lowpass_hz = std::numeric_limits<float>::infinity();
  parameters.bands[0].pre_filters.highpass_hz = -100.f;
  parameters.bands[0].post_filters.lowpass_hz = 1.e8f;
  const auto sanitized = dw::sanitize(parameters);
  REQUIRE(sanitized.input_db == 0.f);
  REQUIRE(sanitized.input_filters.highpass_hz == 20.f);
  REQUIRE(sanitized.input_filters.lowpass_hz == 20000.f);
  REQUIRE(sanitized.bands[0].pre_filters.highpass_hz == 20.f);
  REQUIRE(sanitized.bands[0].post_filters.lowpass_hz == 20000.f);
  parameters = sanitized;
  parameters.input_db = 9.f;
  parameters.input_filters.highpass_enabled = true;
  parameters.input_filters.lowpass_enabled = true;
  parameters.bands[0].pre_filters.highpass_enabled = true;
  parameters.bands[0].pre_filters.lowpass_enabled = true;
  parameters.bands[0].post_filters.highpass_enabled = true;
  parameters.bands[0].post_filters.lowpass_enabled = true;
  parameters.feedback_enabled = true;
  parameters.matrix[0] = 0.4f;
  dw::Engine reused;
  prepare(reused, parameters);
  for(double rate : {8000., 96000., 44100.})
  {
    CAPTURE(rate);
    for(int sample = 0; sample < 1000; ++sample)
      reused.process(stimulus(sample));
    reused.prepare(rate);
    dw::Engine fresh;
    fresh.setParameters(parameters);
    fresh.prepare(rate);
    for(int sample = 0; sample < 4096; ++sample)
    {
      const float input = sample == 3 ? std::numeric_limits<float>::quiet_NaN()
                          : sample == 19 ? std::numeric_limits<float>::infinity()
                                         : stimulus(sample);
      const float output = reused.process(input);
      REQUIRE(dw::finite(output));
      REQUIRE(output == fresh.process(input));
      REQUIRE(reused.feedbackSample(0, 1) == fresh.feedbackSample(0, 1));
    }
    dw::FilterPair filter;
    dw::FilterParameters controls;
    controls.lowpass_hz = 20000.f;
    filter.prepare(controls, rate * 2.);
    REQUIRE(filter.lowpass[0].coefficient.value
            == dw::Lowpass::coefficientFor(float(std::min(20000., rate * 0.45)), rate * 2.));
  }
}

TEST_CASE("Dynamic waveshaper mono adapter preserves independent channel state through reprepare",
          "[dynamic_waveshaper][polyphony]")
{
  using Effect = ao::DynamicWaveshaper;
  constexpr int frames = 128;
  const auto configure = [](Effect& effect) {
    const auto bandControls = [](auto& band) {
      band.algorithm.value = Effect::Algorithm::Soft;
      band.drive.value = 18.f;
      band.dynamics_enabled.value = true;
      band.attack.value = 2.f;
      band.threshold.value = -30.f;
      band.ratio.value = 4.f;
    };
    bandControls(effect.inputs.band1);
    bandControls(effect.inputs.band2);
    bandControls(effect.inputs.band3);
    effect.inputs.inputlevel.value = 6.f;
    effect.inputs.input_filters.highpass_enabled.value = true;
    effect.inputs.input_filters.highpass_hz.value = 70.f;
    effect.inputs.input_filters.lowpass_enabled.value = true;
    effect.inputs.input_filters.lowpass_hz.value = 8000.f;
    effect.inputs.band1_filters.pre.highpass_enabled.value = true;
    effect.inputs.band1_filters.pre.highpass_hz.value = 45.f;
    effect.inputs.band1_filters.post.lowpass_enabled.value = true;
    effect.inputs.band1_filters.post.lowpass_hz.value = 900.f;
    effect.inputs.band2_filters.pre.lowpass_enabled.value = true;
    effect.inputs.band2_filters.pre.lowpass_hz.value = 4000.f;
    effect.inputs.band2_filters.post.highpass_enabled.value = true;
    effect.inputs.band2_filters.post.highpass_hz.value = 150.f;
    effect.inputs.band3_filters.post.lowpass_enabled.value = true;
    effect.inputs.band3_filters.post.lowpass_hz.value = 12000.f;
    effect.inputs.feedback_enabled.value = true;
    effect.inputs.route11.value = 0.6f;
    effect.inputs.route12.value = -0.4f;
    effect.inputs.route21.value = 0.5f;
    effect.inputs.route23.value = -0.3f;
    effect.inputs.route32.value = 0.4f;
    effect.inputs.outputlevel.value = -9.f;
  };

  avnd::effect_container<Effect> effect;
  avnd::process_adapter<Effect> processor;
  effect.init_channels(1, 1);
  for(auto state : effect.full_state())
    configure(state.effect);

  // Growing copies controls into new instances; reprepare must reset both
  // retained and new channel histories. Disconnecting must retain controls.
  for(int channels : {1, 2, 4, 0, 2, 1})
  {
    CAPTURE(channels);
    const double rate = channels == 4 ? 96000. : 48000.;
    avnd::process_setup setup{
        .input_channels = channels,
        .output_channels = channels,
        .frames_per_buffer = frames,
        .rate = rate};
    processor.allocate_buffers(setup, double{});
    effect.init_channels(channels, channels);
    avnd::prepare(effect, setup);
    std::array<Effect, 4> references;
    std::array<std::array<double, frames>, 4> inputs{}, outputs{}, expected{};
    std::array<double*, 4> inPointers{}, outPointers{};
    std::array<double, 4> energies{};
    for(int channel = 0; channel < channels; ++channel)
    {
      configure(references[channel]);
      references[channel].prepare({.rate = rate});
      references[channel].inputs.audio.channel = inputs[channel].data();
      references[channel].outputs.audio.channel = expected[channel].data();
      inPointers[channel] = inputs[channel].data();
      outPointers[channel] = outputs[channel].data();
    }
    for(int block = 0; block < 24; ++block)
    {
      for(int channel = 0; channel < channels; ++channel)
      {
        const bool silent = channels > 1 && channel == channels - 1;
        for(int sample = 0; sample < frames; ++sample)
        {
          const int position = block * frames + sample;
          inputs[channel][sample] = silent ? 0.
              : (0.15 + 0.07 * channel) * std::sin(position * (0.017 + 0.031 * channel));
          outputs[channel][sample] = 123.; // Missing channel writes cannot pass.
        }
        references[channel](frames);
      }
      processor.process(
          effect, avnd::span<double*>{inPointers.data(), std::size_t(channels)},
          avnd::span<double*>{outPointers.data(), std::size_t(channels)}, frames);
      for(int channel = 0; channel < channels; ++channel)
        for(int sample = 0; sample < frames; ++sample)
        {
          CAPTURE(channel, block, sample);
          const double output = outputs[channel][sample];
          REQUIRE(dw::finite(output));
          REQUIRE(output == Catch::Approx(expected[channel][sample]).margin(2.e-6));
          if(channels > 1 && channel == channels - 1)
            REQUIRE(output == Catch::Approx(0.).margin(3.e-6));
          energies[channel] += output * output;
        }
    }
    for(int channel = 0; channel < channels; ++channel)
      if(channels == 1 || channel != channels - 1)
        REQUIRE(energies[channel] > 0.01);
  }
}

TEST_CASE("Dynamic waveshaper processing and parameter changes do not allocate",
          "[dynamic_waveshaper][engine]")
{
  auto parameters = singleBand();
  for(auto& band : parameters.bands)
  {
    band.enabled = true;
    band.dynamics_enabled = true;
    band.begin_enabled = band.end_enabled = true;
    band.pre_filters.highpass_enabled = band.post_filters.lowpass_enabled = true;
  }
  parameters.feedback_enabled = true;
  parameters.matrix = {0.5f, -0.5f, 0.25f, 0.f, 1.f, -1.f, 0.3f, 0.f, 0.7f};
  parameters.input_filters.highpass_enabled = parameters.input_filters.lowpass_enabled = true;
  dw::Engine engine;
  prepare(engine, parameters);

  using Effect = ao::DynamicWaveshaper;
  Effect effect;
  effect.prepare({.rate = sampleRate});
  std::array<double, 256> in{}, out{};
  effect.inputs.audio.channel = in.data();
  effect.outputs.audio.channel = out.data();
  effect.inputs.feedback_enabled.value = true;
  effect.inputs.route12.value = 0.5f;
  effect.inputs.band1.dynamics_enabled.value = true;

  counting = true;
  allocations = 0;
  for(int algorithm = 0; algorithm < dw::algorithm_count; ++algorithm)
  {
    for(auto& band : parameters.bands)
    {
      band.algorithm = (algorithm + int(&band - parameters.bands.data())) % dw::algorithm_count;
      band.drive_db = algorithm % 2 ? 36.f : -24.f;
      band.begin_hz = 100.f + 10.f * algorithm;
      band.post_filters.lowpass_hz = 20000.f - 300.f * algorithm;
    }
    parameters.input_db = algorithm % 3 ? 12.f : -12.f;
    engine.setParameters(parameters);
    for(int sample = 0; sample < 256; ++sample)
      engine.process(sample == 17 ? std::numeric_limits<float>::quiet_NaN()
                                  : 8.f * stimulus(sample));

    effect.inputs.band1.algorithm.value
        = decltype(effect.inputs.band1.algorithm.value)(algorithm);
    effect.inputs.band2.drive.value = algorithm % 2 ? 36.f : 0.f;
    effect.inputs.outputlevel.value = algorithm % 2 ? -6.f : 6.f;
    for(int sample = 0; sample < int(in.size()); ++sample)
      in[sample] = sample == 33 ? 1.e300 : 0.5 * stimulus(sample + 256 * algorithm);
    effect(int(in.size()));
  }
  counting = false;
  REQUIRE(allocations.load() == 0);
}
