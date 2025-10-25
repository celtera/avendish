#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <DspFilters/Filter.h>
#include <DspFilters/RBJ.h>
#include <DspFilters/SmoothedFilter.h>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>

namespace Dsp
{
template <class StateType>
class DynamicChannelsState
{
public:
  DynamicChannelsState() { }

  int getNumChannels() const noexcept { return m_state.size(); }

  void reset(int channels)
  {
    m_state.resize(channels);
    for(int i = 0, N = m_state.size(); i < N; ++i)
      m_state[i].reset();
  }

  StateType& operator[](int index)
  {
    assert(index >= 0 && index < m_state.size());
    return m_state[index];
  }

  template <class Filter, typename Sample>
  void process(int numSamples, Sample* const* arrayOfChannels, Filter& filter)
  {
    for(int i = 0, N = m_state.size(); i < N; ++i)
      filter.process(numSamples, arrayOfChannels[i], m_state[i]);
  }

private:
  std::vector<StateType> m_state;
};

template <class DesignClass, class StateType = DirectFormII>
class SmoothedDynamicFilterDesign : public FilterDesignBase<DesignClass>
{
public:
  using filter_type_t = FilterDesignBase<DesignClass>;

  SmoothedDynamicFilterDesign(int transitionSamples)
      : m_transitionSamples(transitionSamples)
      , m_remainingSamples(-1) // first time flag
  {
  }

  int getNumChannels() override { return m_state.getNumChannels(); }

  void prepare(int channels) { m_state.reset(channels); }

  // Process a block of samples.
  template <typename Sample>
  void processBlock(int numSamples, Sample* const* destChannelArray)
  {
    const int numChannels = this->getNumChannels();

    // If this goes off it means setup() was never called
    assert(m_remainingSamples >= 0);

    // first handle any transition samples
    int remainingSamples = std::min(m_remainingSamples, numSamples);

    if(remainingSamples > 0)
    {
      // interpolate parameters for each sample
      const double t = 1. / m_remainingSamples;
      double dp[maxParameters];
      for(int i = 0; i < DesignClass::NumParams; ++i)
        dp[i] = (this->getParams()[i] - m_transitionParams[i]) * t;

      for(int n = 0; n < remainingSamples; ++n)
      {
        for(int i = DesignClass::NumParams; --i >= 0;)
          m_transitionParams[i] += dp[i];

        m_transitionFilter.setParams(m_transitionParams);

        for(int i = numChannels; --i >= 0;)
        {
          Sample* dest = destChannelArray[i] + n;
          *dest = this->m_state[i].process(*dest, m_transitionFilter);
        }
      }

      m_remainingSamples -= remainingSamples;

      if(m_remainingSamples == 0)
        m_transitionParams = this->getParams();
    }

    // do what's left
    if(numSamples - remainingSamples > 0)
    {
      // no transition
      for(int i = 0; i < numChannels; ++i)
      {
        this->m_design.process(
            numSamples - remainingSamples, destChannelArray[i] + remainingSamples,
            this->m_state[i]);
      }
    }
  }

  void reset() override
  {
    m_remainingSamples = -1;
    m_state.reset(m_state.getNumChannels());
  }

  void process(int numSamples, float* const* arrayOfChannels) override { assert(false); }

  void process(int numSamples, double* const* arrayOfChannels) override
  {
    assert(false);
  }

protected:
  void doSetParams(const Params& parameters) override
  {
    if(m_remainingSamples >= 0)
    {
      m_remainingSamples = m_transitionSamples;
    }
    else
    {
      // first time
      m_remainingSamples = 0;
      m_transitionParams = parameters;
    }

    filter_type_t::doSetParams(parameters);
  }

protected:
  DynamicChannelsState<typename DesignClass::template State<StateType>> m_state;

  Params m_transitionParams{};
  DesignClass m_transitionFilter{};
  int m_transitionSamples{};
  int m_remainingSamples{}; // remaining transition samples
};

}

namespace ao
{
/**
 * @brief Simple audio filter
 */
template <typename Impl, typename Inputs>
struct Filter
{
public:
  halp_meta(category, "Audio/Effects/Filters")
  halp_meta(author, "Vinnie Falco, DSPFilters library")

  Inputs inputs;

  struct
  {
    halp::dynamic_audio_bus<"Out", double> audio;
  } outputs;

  void filterSetup()
  {
    if(setup.rate <= 0)
      return;

    Dsp::Params params;
    params[0] = setup.rate;
    params[1] = inputs.control1;
    if constexpr(requires { inputs.control2; })
      params[2] = inputs.control2;
    if constexpr(requires { inputs.control3; })
      params[3] = inputs.control3;

    filter.setParams(params);
  }

  halp::setup setup;
  void prepare(halp::setup s) noexcept
  {
    setup = s;
    filterSetup();
    filter.prepare(std::min(s.input_channels, s.output_channels));
  }

  using tick = halp::tick;
  void operator()(halp::tick tck) noexcept
  {
    const int C = inputs.audio.channels;

    if(C > 0)
    {
      if(C != this->filter.getNumChannels())
        this->filter.prepare(C);

      filter.processBlock(tck.frames, inputs.audio.samples);
      for(int i = 0; i < C; i++)
        std::copy_n(inputs.audio.samples[i], tck.frames, outputs.audio.samples[i]);
    }
  }

private:
  Dsp::SmoothedDynamicFilterDesign<Impl> filter{128};
};

struct UpdateParam
{
  void update(auto& self) { self.filterSetup(); }
};

struct CutoffParam
    : halp::knob_f32<"Cutoff", halp::range{20, 20000, 400}>
    , UpdateParam
{
  using mapper = halp::log_mapper<std::ratio<85, 100>>;
};
struct FreqParam
    : halp::knob_f32<"Frequency", halp::range{20, 20000, 400}>
    , UpdateParam
{
  using mapper = halp::log_mapper<std::ratio<85, 100>>;
};
struct ResoParam
    : halp::knob_f32<"Resonance", halp::range{0.01, 16., 1.}>
    , UpdateParam
{
};
struct BandwidthParam
    : halp::knob_f32<"Bandwidth", halp::range{0.01, 2., 1.}>
    , UpdateParam
{
};
struct GainParam
    : halp::knob_f32<"Gain", halp::range{-24., 24., 1.}>
    , UpdateParam
{
};
struct SlopeParam
    : halp::knob_f32<"Slope", halp::range{0.25, 4., 1.}>
    , UpdateParam
{
};

struct LowpassInputs
{
  halp::dynamic_audio_bus<"In", double> audio;
  CutoffParam control1;
  ResoParam control2;
};

struct Lowpass : Filter<Dsp::RBJ::Design::LowPass, LowpassInputs>
{
  halp_meta(name, "Lowpass")
  halp_meta(c_name, "dspfilters_lowpass")
  halp_meta(description, "Low-pass audio filter")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#lowpass")
  halp_meta(uuid, "8e768065-4c9b-4cb2-aee3-e48a605b05de")
};

struct HighpassInputs
{
  halp::dynamic_audio_bus<"In", double> audio;
  CutoffParam control1;
  ResoParam control2;
};

struct Highpass : Filter<Dsp::RBJ::Design::HighPass, HighpassInputs>
{
  halp_meta(name, "Highpass")
  halp_meta(c_name, "dspfilters_highpass")
  halp_meta(description, "High-pass audio filter")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#highpass")
  halp_meta(uuid, "c673eeab-fbee-4f2b-89f6-22e84fd5e709")
};

struct BandpassInputs
{
  halp::dynamic_audio_bus<"In", double> audio;

  FreqParam control1;
  BandwidthParam control2;
};

struct Bandpass : Filter<Dsp::RBJ::Design::BandPass1, BandpassInputs>
{
  halp_meta(name, "Bandpass")
  halp_meta(c_name, "dspfilters_bandpass")
  halp_meta(description, "Band-pass audio filter")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#bandpass")
  halp_meta(uuid, "c250f18a-b672-4ec5-8539-d29f00020845")
};

struct Bandstop : Filter<Dsp::RBJ::Design::BandStop, BandpassInputs>
{
  halp_meta(name, "Bandstop")
  halp_meta(c_name, "dspfilters_bandstop")
  halp_meta(description, "Band-stop audio filter")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#bandstop")
  halp_meta(uuid, "795e9733-7c74-4c01-a390-d6fdcc05c760")
};

struct LowshelfInputs
{
  halp::dynamic_audio_bus<"In", double> audio;
  CutoffParam control1;
  GainParam control2;
  SlopeParam control3;
};
struct Lowshelf : Filter<Dsp::RBJ::Design::LowShelf, LowshelfInputs>
{
  halp_meta(name, "Lowshelf")
  halp_meta(c_name, "dspfilters_lowshelf")
  halp_meta(description, "Low-shelf audio filter")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#lowshelf")
  halp_meta(uuid, "fe15d270-7671-44d3-8f67-f038f1ff1d43")
};

struct HighshelfInputs
{
  halp::dynamic_audio_bus<"In", double> audio;
  CutoffParam control1;
  GainParam control2;
  SlopeParam control3;
};
struct Highshelf : Filter<Dsp::RBJ::Design::HighShelf, HighshelfInputs>
{
  halp_meta(name, "Highshelf")
  halp_meta(c_name, "dspfilters_highshelf")
  halp_meta(description, "High-shelf audio filter")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#highshelf")
  halp_meta(uuid, "63727018-e3b2-4e61-b7b1-f1978554db56")
};

struct BandshelfInputs
{
  halp::dynamic_audio_bus<"In", double> audio;
  FreqParam control1;
  GainParam control2;
  BandwidthParam control3;
};
struct Bandshelf : Filter<Dsp::RBJ::Design::BandShelf, BandshelfInputs>
{
  halp_meta(name, "Bandshelf")
  halp_meta(c_name, "dspfilters_bandshelf")
  halp_meta(description, "Band-shelf audio filter")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#bandshelf")
  halp_meta(uuid, "e8f05473-2779-457e-916f-4bfe2771d573")
};

}
