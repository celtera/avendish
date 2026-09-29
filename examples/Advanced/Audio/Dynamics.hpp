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
#include <cstdint>
#include <numeric>
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
  static double makeup_gain(float m) noexcept { return std::max(0.f, 1.f + m); }

private:
  double m_ampEnvelope = 0;
  double m_gainEnvelope = 1;
  std::vector<gam::Delay<double, gam::ipl::Linear, halp::compat::gamma_domain>>
      m_lookahead;
};

/**
 * @brief Lookahead brickwall peak limiter
 *
 * The input times the input gain comes out delayed by the lookahead L and
 * never above the ceiling. The gain computer is a hard knee: min(1, ceiling /
 * peak), with the peak linked over all channels and the sidechain. It is held
 * at its minimum over the last A samples, released with a one-pole, then
 * averaged over A samples: A = attack <= L + 1 is the length of the gain ramp.
 * The detector reads the peaks L + 1 - A samples late so that the ramp ends on
 * the sample that needs it: each averaged gain is then at most the gain
 * computed for the sample it multiplies. The final clamp only catches
 * rounding and ceiling decreases while a ramp is in flight.
 */
struct PeakLimiter
{
  //! Longest lookahead: the size of the delay lines
  static constexpr double max_lookahead = 0.1;

  void prepare(halp::setup info)
  {
    const int channels = std::max(info.input_channels, 1);
    // prepare() also re-runs from the audio thread when the buffer size grows:
    // the lines are only reallocated for a new rate or more channels.
    if(info.rate != m_rate || channels > m_channels)
    {
      m_rate = info.rate;
      allocate(channels);
    }
    reset();
  }

  //! Limits `in.audio` into `out` with the controls of `in`
  template <typename Inputs>
  void run(int frames, const Inputs& in, double** out)
  {
    const int channels = in.audio.channels;
    if(channels == 0)
      return;
    if(channels > m_channels)
    {
      // Only when the host did not prepare() for this channel count
      allocate(channels);
      reset();
    }

    const double drive = finite_or(1. + in.makeup.value, 1., 0., 1e6);
    const double ceiling = finite_or(in.threshold.value, 0., 0., 1e6);
    const double release = finite_or(in.release.value, 0., 0., 1e6) * m_rate;
    const double release_coef
        = release > 0. ? std::exp(-6.907755278982137 / release) : 0.;
    const int lookahead = std::min(
        int(std::lround(finite_or(in.lookahead.value, 0., 0., max_lookahead) * m_rate)),
        m_capacity - 2);
    const int attack = std::clamp(
        int(std::lround(finite_or(in.attack.value, 0., 0., max_lookahead) * m_rate)), 1,
        lookahead + 1);
    if(lookahead != m_lookahead || attack != m_attack)
      reconfigure(lookahead, attack, ceiling);

    const int C = m_capacity;
    const int L = m_lookahead;
    const int A = m_attack;
    const int64_t detector_delay = L + 1 - A;
    double* const audio = m_audio.data();
    double* const peaks = m_peaks.data();
    double* const box = m_box.data();
    int read = m_write - L;
    if(read < 0)
      read += C;

    for(int i = 0; i < frames; i++)
    {
      double peak = 0.;
      for(int c = 0; c < channels; c++)
      {
        double x = drive * in.audio.samples[c][i];
        if(!std::isfinite(x))
          x = 0.;
        audio[c * C + m_write] = x;
        peak = std::max(peak, std::abs(x));
      }
      for(int c = 0; c < in.sidechain.channels; c++)
      {
        const double s = std::abs(in.sidechain.samples[c][i]);
        if(std::isfinite(s))
          peak = std::max(peak, s);
      }
      peaks[m_write] = peak;

      push_peak(m_now - detector_delay, A);
      const double g = gain(peaks[position(m_queue[m_queue_head])], ceiling);
      m_env = g < m_env ? g : g + release_coef * (m_env - g);

      m_box_sum += m_env - box[m_box_index];
      box[m_box_index] = m_env;
      if(++m_box_index == A)
      {
        m_box_index = 0;
        m_box_sum = std::accumulate(box, box + A, 0.);
      }
      const double smoothed = m_box_sum / A;

      for(int c = 0; c < channels; c++)
        out[c][i] = std::clamp(audio[c * C + read] * smoothed, -ceiling, ceiling);

      if(++m_write == C)
        m_write = 0;
      if(++read == C)
        read = 0;
      ++m_now;
    }
  }

private:
  static double finite_or(double v, double fallback, double lo, double hi) noexcept
  {
    return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
  }

  static double gain(double peak, double ceiling) noexcept
  {
    return peak > ceiling ? ceiling / peak : 1.;
  }

  int position(int64_t sample) const noexcept { return int(sample % m_capacity); }

  //! Running maximum of the peaks over [sample - window + 1, sample]
  void push_peak(int64_t sample, int window) noexcept
  {
    const int C = m_capacity;
    const double v = m_peaks[position(sample)];
    while(m_queue_size > 0)
    {
      int back = m_queue_head + m_queue_size - 1;
      if(back >= C)
        back -= C;
      if(m_peaks[position(m_queue[back])] > v)
        break;
      --m_queue_size;
    }
    int slot = m_queue_head + m_queue_size;
    if(slot >= C)
      slot -= C;
    m_queue[slot] = sample;
    ++m_queue_size;
    while(m_queue[m_queue_head] <= sample - window)
    {
      if(++m_queue_head == C)
        m_queue_head = 0;
      --m_queue_size;
    }
  }

  //! New lookahead or attack: rebuilds the running maximum and restarts the
  //! ramp from a gain low enough for every sample already in the delay line.
  void reconfigure(int lookahead, int attack, double ceiling) noexcept
  {
    m_lookahead = lookahead;
    m_attack = attack;

    m_queue_head = 0;
    m_queue_size = 0;
    const int64_t next = m_now - (lookahead + 1 - attack);
    for(int64_t s = next - attack + 1; s < next; s++)
      push_peak(s, attack);

    double pending = 0.;
    for(int64_t s = m_now - lookahead; s < m_now; s++)
      pending = std::max(pending, m_peaks[position(s)]);
    m_env = std::min(m_env, gain(pending, ceiling));

    std::fill_n(m_box.begin(), attack, m_env);
    m_box_sum = attack * m_env;
    m_box_index = 0;
  }

  void allocate(int channels)
  {
    m_channels = channels;
    m_capacity = int(std::ceil(max_lookahead * m_rate)) + 2;
    m_audio.assign(std::size_t(channels) * m_capacity, 0.);
    m_peaks.assign(m_capacity, 0.);
    m_box.assign(m_capacity, 0.);
    m_queue.assign(m_capacity, 0);
  }

  void reset() noexcept
  {
    std::ranges::fill(m_audio, 0.);
    std::ranges::fill(m_peaks, 0.);
    m_env = 1.;
    m_now = m_capacity;
    m_write = 0;
    m_lookahead = -1;
    m_attack = -1;
  }

  double m_rate{};
  int m_channels{};
  int m_capacity{};
  std::vector<double> m_audio;
  std::vector<double> m_peaks;
  std::vector<double> m_box;
  std::vector<int64_t> m_queue;
  int m_queue_head{};
  int m_queue_size{};
  int64_t m_now{};
  int m_write{};
  int m_lookahead{-1};
  int m_attack{-1};
  double m_env{1.};
  double m_box_sum{};
  int m_box_index{};
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
struct Limiter : PeakLimiter
{
  halp_meta(name, "Limiter (old)")
  halp_meta(category, "Audio/Effects")
  halp_meta(author, "ofxTonic library authors")
  halp_meta(c_name, "ofxtonic_limiter")
  halp_meta(
      description, "Lookahead peak limiter: the output never exceeds the threshold")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-effects.html#limiter")
  halp_flag(deprecated);
  halp_meta(uuid, "7570d058-d243-4c74-84cc-f5b3f5d752bd")
  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    halp::dynamic_audio_bus<"Sidechain", double> sidechain;

    //! Input gain (1 + value) pushing the signal into the limiter
    halp::knob_f32<"Makeup", halp::range{0, 30, 0}> makeup;
    halp::knob_f32<"Attack", halp::range{0, 1, 0.003}> attack;
    halp::knob_f32<"Relase", halp::range{0, 1, 0.08}> release;
    halp::knob_f32<"Threshold", halp::range{0., 1., 0.98}> threshold;
    halp::knob_f32<"Lookahead", halp::range{0.001, 0.005, 0.003}> lookahead;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Output", double> audio;
  } outputs;

  void operator()(int frames) { run(frames, inputs, outputs.audio.samples); }
};

struct Limiter_v2 : PeakLimiter
{
  halp_meta(name, "Limiter")
  halp_meta(category, "Audio/Effects")
  halp_meta(author, "ofxTonic library authors")
  halp_meta(c_name, "ofxtonic_limiter_v2")
  halp_meta(
      description, "Lookahead peak limiter: the output never exceeds the threshold")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-effects.html#limiter")
  halp_meta(uuid, "1413ed74-1c9f-4433-976a-f588d4027735")
  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    halp::dynamic_audio_bus<"Sidechain", double> sidechain;

    //! Input gain (1 + value) pushing the signal into the limiter
    halp::knob_f32<"Makeup", halp::range{0, 30, 0}> makeup;
    halp::time_chooser<"Attack", halp::range{0., 1., 0.003}> attack;
    //! Synced to a note value, the gain comes back in time with the beat.
    halp::time_chooser<"Release", halp::range{0., 2., 0.08}> release;
    halp::knob_f32<"Threshold", halp::range{0., 1., 0.98}> threshold;
    halp::knob_f32<"Lookahead", halp::range{0.001, 0.005, 0.003}> lookahead;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Output", double> audio;
  } outputs;

  void operator()(int frames) { run(frames, inputs, outputs.audio.samples); }
};
}
