#include "Sampler.hpp"

namespace kbng
{

sampler::sampler(halp::soundfile_view<float>& sf)
    : sf{sf}
{
  for(int i = 0; i < max_voices; i++)
    available_voices.push_back(&voice_pool[i]);
}

voice& sampler::trigger()
{
  voice* ptr{};
  if(BOOST_UNLIKELY(available_voices.empty()))
  {
    ptr = voices.front();
    voices.erase(voices.begin());
  }
  else
  {
    ptr = available_voices.back();
    available_voices.pop_back();
  }

  if(!ptr->rubberBand || ptr->rubberBand->getChannelCount() != sf.channels)
  {
    using opt_t = RubberBand::RubberBandStretcher::Option;
    using preset_t = RubberBand::RubberBandStretcher::PresetOption;
    uint32_t preset = 0;
    preset |= preset_t::PercussiveOptions;
    preset |= opt_t::OptionProcessRealTime;
    preset |= opt_t::OptionThreadingNever;
    preset |= opt_t::OptionEngineFaster;
    //preset |= opt_t::OptionEngineFiner;
    //preset |= opt_t::OptionPitchHighQuality;
    preset |= opt_t::OptionPitchHighSpeed;
    //preset |= opt_t::OptionPitchHighConsistency;

    delete ptr->rubberBand;
    ptr->rubberBand = new RubberBand::RubberBandStretcher(sf.rate, sf.channels, preset);
  }
  ptr->rubberBand->reset();
  ptr->position = 0;
  ptr->read_position = 0;
  voices.push_back(ptr);
  return *ptr;
}

void sampler::filter(voice& v, double* (&arr)[2])
{
  // Apply filters
  double fenv = v.filt_adsr();

  double filt_env = v.flt_envelope ? fenv * (1. + v.filt_track) : 0.;
  Dsp::Params params;

  // HPF
  params[0] = sf.rate;                                                // sample rate
  params[1] = std::clamp(v.hpCutoff - 1000. * filt_env, 20., 20000.); // cutoff
  params[2] = v.hpRes;                                                // Q
  v.highpassFilter.setParams(params);

  // LPF
  params[1] = std::clamp(v.lpCutoff + 1000. * filt_env, 20., 20000.); // cutoff
  params[2] = v.lpRes;                                                // Q
  v.lowpassFilter.setParams(params);

  v.lowpassFilter.process(1, arr);
  v.highpassFilter.process(1, arr);

  // Apply amplitude
  double aenv = v.amp_adsr();
  const double amp_env = v.amp_envelope ? aenv * (1. + v.ampl_track) : 1.;
  *arr[0] *= amp_env;
  *arr[1] *= amp_env;
}

void sampler::process_frame(voice& v, double& out_l, double& out_r)
{
  // 1. Feed frames to rubberband until we can get one out
  float* samples = (float*)alloca(sizeof(float) * sf.channels);
  float** in_arr = (float**)alloca(sizeof(float) * sf.channels);
  for(int i = 0; i < sf.channels; i++)
    in_arr[i] = &samples[i];

  auto penv = v.pitch_adsr();

  auto read_one_frame = [&] {
    if(v.read_position < sf.frames)
    {
      for(int i = 0; i < sf.channels; i++)
        samples[i] = sf.data[i][v.read_position];
      v.read_position++;
    }
    else
    {
      v.amp_adsr.release();
      for(int i = 0; i < sf.channels; i++)
        samples[i] = 0.f;
    }
  };

  // Process pitch envelope
  if(v.pitch_envelope || v.pitch != 1.)
  {
    if(v.pitch_track >= 0.)
      penv *= v.pitch_track;
    else
      penv /= -v.pitch_track;
    auto pitch_env = std::clamp(v.pitch + 5.0 * penv * v.pitch_track, 1. / 5., 5.);
    v.rubberBand->setPitchScale(pitch_env);

    while(v.rubberBand->available() == 0)
    {
      read_one_frame();
      v.rubberBand->process(in_arr, 1, false);
    }
    v.rubberBand->retrieve(in_arr, 1);
  }
  else
  {
    read_one_frame();
  }

  double ld = 0., rd = 0.;
  if(sf.channels == 1)
  {
    ld = rd = samples[0];
  }
  else
  {
    ld = samples[0];
    rd = samples[1];
  }
  double* in_arr_d[2]{&ld, &rd};

  // 3. Process
  filter(v, in_arr_d);

  out_l += ld * volume;
  out_r += rd * volume;
}

void sampler::sample_to_2(int frames, double** buffers)
{
  for(auto it = this->voices.begin(); it != this->voices.end();)
  {
    voice& v = **it;
    const int64_t start = v.position;

    // Copy it at the given position for each output
    {
      auto out_l = buffers[0];
      auto out_r = buffers[1];

      for(int j = 0; j < frames; j++)
      {
        if(start + j >= 0)
        {
          process_frame(v, out_l[j], out_r[j]);
        }
      }
    }
    v.position += frames;

    bool finished = v.amp_envelope ? v.amp_adsr.done() : v.read_position >= sf.frames;
    if(!finished)
    {
      ++it;
    }
    else
    {
      it = this->voices.erase(it);
      this->available_voices.push_back(&v);
    }
  }
}

}
