#pragma once
#include <DspFilters/Filter.h>
#include <DspFilters/RBJ.h>
#include <DspFilters/SmoothedFilter.h>
#include <Gamma/Envelope.h>
#include <RubberBandStretcher.h>
#include <boost/container/static_vector.hpp>
#include <halp/audio.hpp>
#include <halp/compat/gamma.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/soundfile_port.hpp>

#include <cinttypes>

namespace kbng
{
struct voice
{
  double ampl_track{};
  double filt_track{};
  double pitch_track{};
  double pitch{};
  int64_t position{};
  int64_t read_position{};

  double lpCutoff{};
  double lpRes{};
  double hpCutoff{};
  double hpRes{};
  bool amp_envelope{};
  bool flt_envelope{};
  bool pitch_envelope{};

  gam::ADSR<double, double, halp::compat::gamma_domain> amp_adsr;
  gam::ADSR<double, double, halp::compat::gamma_domain> pitch_adsr;
  gam::ADSR<double, double, halp::compat::gamma_domain> filt_adsr;

  static constexpr auto chans = 2;
  Dsp::SmoothedFilterDesign<Dsp::RBJ::Design::LowPass, chans> lowpassFilter{128};
  Dsp::SmoothedFilterDesign<Dsp::RBJ::Design::HighPass, chans> highpassFilter{128};

  RubberBand::RubberBandStretcher* rubberBand{};
};

struct sampler
{
  halp::soundfile_view<float>& sf;

  static constexpr const int max_voices = 8;
  voice voice_pool[max_voices];
  double volume{1.};

  boost::container::static_vector<voice*, max_voices> available_voices;
  boost::container::static_vector<voice*, max_voices> voices;

  explicit sampler(halp::soundfile_view<float>&);

  voice& trigger();

  void filter(voice& v, double* (&arr)[2]);
  void process_frame(voice& v, double& out_l, double& out_r);
  void sample_to_2(int frames, double** buffers);
};

template <halp::static_string lit, typename T = float>
struct sample_port
    : public halp::soundfile_port<lit, T>
    , public sampler
{
  sample_port() noexcept
      : sampler{this->soundfile}
  {
  }
  ~sample_port() = default;

  void update(const auto& obj)
  {
    available_voices.insert(available_voices.end(), voices.begin(), voices.end());
    voices.clear();
  }

  void sample(int frames_to_write, int numchannels, double** buffers)
  {
    if(this->voices.empty())
      return;
    if(this->channels() == 0)
      return;
    if(frames_to_write == 0 || numchannels == 0)
      return;

    if(numchannels == 2)
      sample_to_2(frames_to_write, buffers);
  }
};
}
