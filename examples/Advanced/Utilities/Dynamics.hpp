#pragma once
#include <Gamma/Delay.h>
#include <boost/config.hpp>
#include <boost/math/constants/constants.hpp>
#include <cmath>
#include <halp/audio.hpp>
#include <halp/compat/gamma.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <vector>

namespace ao
{
struct DynamicsProcessor
{
  halp::setup info;
  void prepare(halp::setup info) noexcept
  {
    this->info = info;
    this->m_ampEnvelope = 0;
    this->m_gainEnvelope = 1;

    this->m_lookahead.clear();
  }

  float t60ToOnePoleCoef(double t60s)
  {
    using namespace std;
    return exp(-1.0 / ((t60s / 6.91) * info.rate));
  }

  void onePoleLPFTick(double input, double& output, double coef)
  {
    using namespace std;
    output = ((1.0f - coef) * input) + (coef * output);
  }
  void process(
      int frames, double** const in, double** const sc_in, double** const out,
      int in_channels, int sc_channels, double attackCoef, double releaseCoef,
      double threshold, double ratio, double lookaheadTime, double makeup)
  {
    using namespace std;

    // Prepare delay lines
    if(std::ssize(this->m_lookahead) != in_channels)
    {
      this->m_lookahead.resize(in_channels);
      for(int c = 0; c < in_channels; c++)
      {
        m_lookahead[c].set_sample_rate(info.rate);
        m_lookahead[c].maxDelay(0.1, false);
      }
    }

    for(auto& dl : m_lookahead)
    {
      dl.delay(lookaheadTime);
    }

    // Iterate through samples
    const int amp_channels = sc_channels > 0 ? sc_channels : in_channels;
    const auto amp_data = sc_channels > 0 ? sc_in : in;

    {
      double ampInputValue{}, gainValue{}, gainTarget{};
      for(int i = 0; i < frames; i++)
      {
        // Tick input into lookahead delay and get amplitude input value
        ampInputValue = 0;
        for(int c = 0; c < in_channels; c++)
        {
          m_lookahead[c](in[c][i]);
        }

        for(int c = 0; c < amp_channels; c++)
        {
          ampInputValue = max(ampInputValue, abs(amp_data[c][i]));
        }

        // Smooth amplitude input
        if(ampInputValue >= m_ampEnvelope)
        {
          onePoleLPFTick(ampInputValue, m_ampEnvelope, attackCoef);
        }
        else
        {
          onePoleLPFTick(ampInputValue, m_ampEnvelope, releaseCoef);
        }

        // Calculate gain value
        if(m_ampEnvelope <= threshold)
        {
          gainValue = 1.0f;
        }
        else
        {
          // compensate for ratio
          gainTarget = threshold + ((m_ampEnvelope - threshold) / ratio);
          gainValue = gainTarget / m_ampEnvelope;
        }

        // Smooth gain value
        if(gainValue <= m_gainEnvelope)
        {
          onePoleLPFTick(gainValue, m_gainEnvelope, attackCoef);
        }
        else
        {
          onePoleLPFTick(gainValue, m_gainEnvelope, releaseCoef);
        }

        // apply gain
        for(int c = 0; c < in_channels; c++)
        {
          out[c][i] = m_lookahead[c]() * m_gainEnvelope;
        }
      }
    }
  }

  void postprocess_compress(
      int frames, int in_channels, double** out, double makeupGain, double threshold)
  {
    using namespace std;
    for(int c = 0; c < in_channels; c++)
    {
      for(int i = 0; i < frames; i++)
      {
        out[c][i] *= makeupGain;
      }
    }
  }

  double softlimit(double x, double t)
  {
    using namespace std;
    // Designed here: https://www.desmos.com/calculator/gxdswx6rwm
    constexpr double d = 0.03;
    const double A = -1. + (d + t);
    const double B = 1 - (d + t);
    if(BOOST_UNLIKELY(x <= A))
    {
      return A + d * tanh((x - A) / d);
    }
    else if(BOOST_UNLIKELY(x >= B))
    {
      return B + d * tanh((x - B) / d);
    }
    else
    {
      return x;
    }
  }

  void postprocess_limit(
      int frames, int in_channels, double** out, double makeupGain, double threshold)
  {
    using namespace std;
    for(int c = 0; c < in_channels; c++)
    {
      for(int i = 0; i < frames; i++)
      {
        out[c][i] = clamp(makeupGain * out[c][i], -threshold, threshold);
      }
    }
  }

  void postprocess_softlimit(
      int frames, int in_channels, double** out, double makeupGain, double threshold)
  {
    using namespace std;
    for(int c = 0; c < in_channels; c++)
    {
      for(int i = 0; i < frames; i++)
      {
        out[c][i] = softlimit(makeupGain * out[c][i], threshold);
      }
    }
  }

private:
  double m_ampEnvelope = 0;
  double m_gainEnvelope = 1;
  std::vector<gam::Delay<double, gam::ipl::Linear, halp::compat::gamma_domain>>
      m_lookahead;
};

/**
 * @brief Basic dynamics compressor ported from ofxTonic library
 */
struct Compressor : DynamicsProcessor
{
  halp_meta(name, "Compressor")
  halp_meta(category, "Audio/Effects")
  halp_meta(author, "ofxTonic library authors")
  halp_meta(c_name, "ofxtonic_compressor")
  halp_meta(uuid, "352ba9b1-eeab-4408-9b98-aa2c2585508a")
  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    halp::dynamic_audio_bus<"Sidechain", double> sidechain;

    halp::knob_f32<"Makeup", halp::range{0, 30, 0}> makeup;
    halp::knob_f32<"Attack", halp::range{0, 1, 0.001}> attack;
    halp::knob_f32<"Relase", halp::range{0, 1, 0.05}> release;
    halp::knob_f32<"Threshold", halp::range{0., 1., 0.5}> threshold;
    halp::knob_f32<"Ratio", halp::range{0.05, 50., 1.}> ratio;
    halp::knob_f32<"Lookahead", halp::range{0.001, 0.005, 0.001}> lookahead;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Output", double> audio;
  } outputs;

  void operator()(int frames)
  {
    using namespace std;

    const auto in_channels = this->inputs.audio.channels;
    const auto sc_channels = this->inputs.sidechain.channels;
    if(in_channels == 0)
      return;

    // Setup parameters
    const double attackCoef = t60ToOnePoleCoef(max(0.f, inputs.attack.value));
    const double releaseCoef = t60ToOnePoleCoef(max(0.f, inputs.release.value));
    const double threshold = max(0.f, inputs.threshold.value);
    const double ratio = max(0.f, inputs.ratio.value);
    const double lookaheadTime = max(0.f, inputs.lookahead.value);
    const double makeup = max(0.f, 1.f + inputs.makeup.value);

    this->process(
        frames, inputs.audio.samples, inputs.sidechain.samples, outputs.audio.samples,
        in_channels, sc_channels, attackCoef, releaseCoef, threshold, ratio,
        lookaheadTime, makeup);
    this->postprocess_compress(
        frames, in_channels, outputs.audio.samples, makeup, threshold);
  }
};
struct Limiter : DynamicsProcessor
{
  halp_meta(name, "Limiter")
  halp_meta(category, "Audio/Effects")
  halp_meta(author, "ofxTonic library authors")
  halp_meta(c_name, "ofxtonic_limiter")
  halp_meta(uuid, "7570d058-d243-4c74-84cc-f5b3f5d752bd")
  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    halp::dynamic_audio_bus<"Sidechain", double> sidechain;

    halp::knob_f32<"Makeup", halp::range{0, 30, 0}> makeup;
    halp::knob_f32<"Attack", halp::range{0, 1, 0.0001}> attack;
    halp::knob_f32<"Relase", halp::range{0, 1, 0.08}> release;
    halp::knob_f32<"Threshold", halp::range{0., 1., 0.98}> threshold;
    halp::knob_f32<"Lookahead", halp::range{0.001, 0.005, 0.003}> lookahead;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Output", double> audio;
  } outputs;

  void operator()(int frames)
  {
    using namespace std;

    const auto in_channels = this->inputs.audio.channels;
    const auto sc_channels = this->inputs.sidechain.channels;
    if(in_channels == 0)
      return;

    // Setup parameters
    const double attackCoef = t60ToOnePoleCoef(max(0.f, inputs.attack.value));
    const double releaseCoef = t60ToOnePoleCoef(max(0.f, inputs.release.value));
    const double threshold = max(0.f, inputs.threshold.value);
    const double ratio = 9999999999.;
    const double lookaheadTime = max(0.f, inputs.lookahead.value);
    const double makeup = max(0.f, 1.f + inputs.makeup.value);

    this->process(
        frames, inputs.audio.samples, inputs.sidechain.samples, outputs.audio.samples,
        in_channels, sc_channels, attackCoef, releaseCoef, threshold, ratio,
        lookaheadTime, makeup);
    this->postprocess_softlimit(
        frames, in_channels, outputs.audio.samples, makeup, threshold);
  }
};
}
