#pragma once
#include <Gamma/Delay.h>
#include <boost/config.hpp>
#include <boost/math/constants/constants.hpp>
#include <cmath>
#include <halp/audio.hpp>
#include <halp/compat/gamma.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <vector>

namespace ao
{
struct DynamicsProcessor
{
  halp::setup info;
  //! Longest lookahead: the size of the delay lines
  static constexpr double max_lookahead = 0.1;

  void prepare(halp::setup info) noexcept
  {
    // prepare() also re-runs from the audio thread when the buffer size grows:
    // the delay lines are only reallocated for a new rate.
    if(info.rate != this->info.rate)
      this->m_lookahead.clear();
    else
      for(auto& dl : this->m_lookahead)
        dl.zero();

    this->info = info;
    this->m_ampEnvelope = 0;
    this->m_gainEnvelope = 1;
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
      double threshold, double ratio, double lookaheadTime)
  {
    using namespace std;

    // Prepare delay lines
    if(std::ssize(this->m_lookahead) != in_channels)
    {
      this->m_lookahead.resize(in_channels);
      for(int c = 0; c < in_channels; c++)
      {
        m_lookahead[c].set_sample_rate(info.rate);
        m_lookahead[c].maxDelay(max_lookahead, false);
      }
    }

    // Beyond the line, the read position wraps around; at 0 it reads the
    // oldest sample instead of the newest, as the line is read before written.
    const double one_sample = 1. / info.rate;
    lookaheadTime = std::clamp(lookaheadTime, one_sample, max_lookahead - one_sample);
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

  void postprocess_compress(int frames, int in_channels, double** out, double makeupGain)
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

  //! Linear up to a knee below `ceiling`, then a tanh curve reaching it
  //! asymptotically: the output never exceeds the ceiling, which is the
  //! threshold of the gain computer. softlimit() instead tends to 1 - t.
  static double softclip(double x, double ceiling) noexcept
  {
    if(ceiling <= 0.)
      return 0.;
    const double knee = std::min(0.03, 0.5 * ceiling);
    const double lin = ceiling - knee;
    const double ax = std::abs(x);
    if(BOOST_LIKELY(ax <= lin))
      return x;
    return std::copysign(lin + knee * std::tanh((ax - lin) / knee), x);
  }

  void postprocess_softclip(
      int frames, int in_channels, double** out, double makeupGain, double ceiling)
  {
    for(int c = 0; c < in_channels; c++)
      for(int i = 0; i < frames; i++)
        out[c][i] = softclip(makeupGain * out[c][i], ceiling);
  }

  //! Compresses `in` into `out` with the attack, release, threshold and
  //! lookahead controls of `in`; false when there is no input.
  template <typename Inputs>
  bool run(int frames, const Inputs& in, double** out, double ratio)
  {
    const int in_channels = in.audio.channels;
    if(in_channels == 0)
      return false;

    this->process(
        frames, in.audio.samples, in.sidechain.samples, out, in_channels,
        in.sidechain.channels, t60ToOnePoleCoef(std::max(0.f, in.attack.value)),
        t60ToOnePoleCoef(std::max(0.f, in.release.value)),
        std::max(0.f, in.threshold.value), ratio, in.lookahead.value);
    return true;
  }

  //! A ratio of 0 would divide by zero
  static double compression_ratio(float r) noexcept { return std::max(0.05f, r); }
  static constexpr double limiter_ratio = 9999999999.;
  static double makeup_gain(float m) noexcept { return std::max(0.f, 1.f + m); }

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
  halp_meta(name, "Compressor (old)")
  halp_meta(category, "Audio/Effects")
  halp_meta(author, "ofxTonic library authors")
  halp_meta(c_name, "ofxtonic_compressor")
  halp_meta(description, "Dynamics compressor")
  halp_flag(deprecated);
  halp_meta(uuid, "352ba9b1-eeab-4408-9b98-aa2c2585508a")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-effects.html#compressor")
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
    if(run(frames, inputs, outputs.audio.samples, compression_ratio(inputs.ratio)))
      postprocess_compress(
          frames, inputs.audio.channels, outputs.audio.samples,
          makeup_gain(inputs.makeup));
  }
};

struct Compressor_v2 : DynamicsProcessor
{
  halp_meta(name, "Compressor")
  halp_meta(category, "Audio/Effects")
  halp_meta(author, "ofxTonic library authors")
  halp_meta(c_name, "ofxtonic_compressor_v2")
  halp_meta(description, "Dynamics compressor")
  halp_meta(uuid, "3e9688b1-cfb3-4e01-a8ca-c24e189d8572")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-effects.html#compressor")
  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    halp::dynamic_audio_bus<"Sidechain", double> sidechain;

    halp::knob_f32<"Makeup", halp::range{0, 30, 0}> makeup;
    halp::time_chooser<"Attack", halp::range{0., 1., 0.001}> attack;
    //! Synced to a note value, the gain comes back in time with the beat.
    halp::time_chooser<"Release", halp::range{0., 2., 0.05}> release;
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
    if(run(frames, inputs, outputs.audio.samples, compression_ratio(inputs.ratio)))
      postprocess_compress(
          frames, inputs.audio.channels, outputs.audio.samples,
          makeup_gain(inputs.makeup));
  }
};
struct Limiter : DynamicsProcessor
{
  halp_meta(name, "Limiter (old)")
  halp_meta(category, "Audio/Effects")
  halp_meta(author, "ofxTonic library authors")
  halp_meta(c_name, "ofxtonic_limiter")
  halp_meta(description, "Dynamics limiter")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-effects.html#limiter")
  halp_flag(deprecated);
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
    if(run(frames, inputs, outputs.audio.samples, limiter_ratio))
      postprocess_softlimit(
          frames, inputs.audio.channels, outputs.audio.samples,
          makeup_gain(inputs.makeup), std::max(0.f, inputs.threshold.value));
  }
};

struct Limiter_v2 : DynamicsProcessor
{
  halp_meta(name, "Limiter")
  halp_meta(category, "Audio/Effects")
  halp_meta(author, "ofxTonic library authors")
  halp_meta(c_name, "ofxtonic_limiter_v2")
  halp_meta(description, "Dynamics limiter")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-effects.html#limiter")
  halp_meta(uuid, "1413ed74-1c9f-4433-976a-f588d4027735")
  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    halp::dynamic_audio_bus<"Sidechain", double> sidechain;

    halp::knob_f32<"Makeup", halp::range{0, 30, 0}> makeup;
    halp::time_chooser<"Attack", halp::range{0., 1., 0.0001}> attack;
    //! Synced to a note value, the gain comes back in time with the beat.
    halp::time_chooser<"Release", halp::range{0., 2., 0.08}> release;
    halp::knob_f32<"Threshold", halp::range{0., 1., 0.98}> threshold;
    halp::knob_f32<"Lookahead", halp::range{0.001, 0.005, 0.003}> lookahead;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Output", double> audio;
  } outputs;

  void operator()(int frames)
  {
    if(run(frames, inputs, outputs.audio.samples, limiter_ratio))
      postprocess_softclip(
          frames, inputs.audio.channels, outputs.audio.samples,
          makeup_gain(inputs.makeup), std::max(0.f, inputs.threshold.value));
  }
};
}
