#pragma once
#include <halp/audio.hpp>
#include <halp/file_port.hpp>
#include <RubberBandStretcher.h>

#include <cinttypes>

namespace kbng
{
struct voice
{
  double velocity{};
  double pitch{};
  int64_t position{};
};

struct sampler
{
  void trigger(int ts, int vel_byte)
  {
    ts = 0;
    double velocity = vel_byte / 127.;
    voices.push_back(voice{velocity, 0., -ts});
  }

  void sample_1_to_1(
      int frames, std::floating_point auto** buffers, int file_frames, const float** file_buffers)
  {
    // Just take the first channel of the soundfile.
    const auto in = file_buffers[0];

    for(auto it = this->voices.begin(); it != this->voices.end();)
    {
      voice& v = *it;
      // We'll read at this position
      const int64_t start = v.position;

      // Copy it at the given position for each output
      {
        auto out = buffers[0];

        for(int j = 0; j < frames; j++)
        {
          if(start + j >= 0 && start + j < file_frames) [[likely]]
            out[j] += in[start + j];
        }
      }

      v.position += frames;

      if(v.position < file_frames)
        ++it;
      else
        it = this->voices.erase(it);
    }
  }

  void sample_1_to_N(
      int frames, int numchannels, std::floating_point auto** buffers, int file_frames,
      const float** file_buffers)
  {
    // Just take the first channel of the soundfile.
    const auto in = file_buffers[0];

    for(auto it = this->voices.begin(); it != this->voices.end();)
    {
      voice& v = *it;
      // We'll read at this position
      const int64_t start = v.position;

      // Copy it at the given position for each output
      for(int i = 0; i < numchannels; i++)
      {
        // Output buffer for channel i, also a std::span.
        auto out = buffers[i];

        for(int j = 0; j < frames; j++)
        {
          // If we're before the end of the file copy the sample
          if(start + j >= 0 && start + j < file_frames) [[likely]]
            out[j] += in[start + j];
        }
      }

      v.position += frames;

      if(v.position < file_frames)
        ++it;
      else
        it = this->voices.erase(it);
    }
  }

  void sample_2_to_1(
      int frames, std::floating_point auto** buffers, int file_frames, const float** file_buffers)
  {
    for(auto it = this->voices.begin(); it != this->voices.end();)
    {
      voice& v = *it;
      // We'll read at this position
      const int64_t start = v.position;

      // Copy it at the given position for each output
      {
        const auto in_l = file_buffers[0];
        const auto in_r = file_buffers[1];
        auto out = buffers[0];

        for(int j = 0; j < frames; j++)
        {
          if(start + j >= 0 && start + j < file_frames) [[likely]]
          {
            out[j] += (in_l[start + j] + in_r[start + j]) / 2.;
          }
        }
      }

      v.position += frames;

      if(v.position < file_frames)
        ++it;
      else
        it = this->voices.erase(it);
    }
  }
  void sample_2_to_2(
      int frames, std::floating_point auto** buffers, int file_frames, const float** file_buffers)
  {
    for(auto it = this->voices.begin(); it != this->voices.end();)
    {
      voice& v = *it;
      // We'll read at this position
      const int64_t start = v.position;

      // Copy it at the given position for each output
      {
        const auto in_l = file_buffers[0];
        auto out_l = buffers[0];
        const auto in_r = file_buffers[1];
        auto out_r = buffers[1];

        for(int j = 0; j < frames; j++)
        {
          if(start + j >= 0 && start + j < file_frames) [[likely]]
          {
            out_l[j] += in_l[start + j];
            out_r[j] += in_r[start + j];
          }
        }
      }

      v.position += frames;

      if(v.position < file_frames)
        ++it;
      else
        it = this->voices.erase(it);
    }
  }

  void sample_generic(
      int frames, int numchannels, std::floating_point auto** buffers, int file_frames, int file_channels,
      const float** file_buffers)
  {
    // FIXME do better.. but how to adapt e.g. 7 channels in 3?
    sample_1_to_N(frames, numchannels, buffers, file_frames, file_buffers);
  }

  std::vector<voice> voices;
};

template <halp::static_string lit, typename T = float>
struct sample_port
    : public halp::soundfile_port<lit, T>
    , public sampler
{
  ~sample_port()
  {
    delete this->m_rubberBand;
  }
  void update(const auto& obj)
  {
    using opt_t = RubberBand::RubberBandStretcher::Option;
    using preset_t = RubberBand::RubberBandStretcher::PresetOption;
    uint32_t preset = 0;
    preset |= preset_t::PercussiveOptions;
    preset |= opt_t::OptionProcessRealTime;
    preset |= opt_t::OptionThreadingNever;
    preset |= opt_t::OptionEngineFaster;
    preset |= opt_t::OptionPitchHighSpeed;
    // preset |= opt_t::OptionPitchHighConsistency;

    // delete this->m_rubberBand;
    // this->m_rubberBand = new RubberBand::RubberBandStretcher(
    //      this->soundfile.rate, this->channels(), preset);
  }

  void fetch_audio(int numchannels, int frames_to_read, auto** read_buffers)
  {
    switch(this->channels())
    {
      case 1:
        if(numchannels == 1)
          return sample_1_to_1(frames_to_read, read_buffers, this->frames(), this->soundfile.data);
        else if(numchannels == 2)
          return sample_1_to_N(
              frames_to_read, numchannels, read_buffers, this->frames(), this->soundfile.data);
        else
          return sample_generic(
              frames_to_read, numchannels, read_buffers, this->frames(), this->channels(),
              this->soundfile.data);
      case 2:
        if(numchannels == 1)
          return sample_2_to_1(frames_to_read, read_buffers, this->frames(), this->soundfile.data);
        else if(numchannels == 2)
          return sample_2_to_2(frames_to_read, read_buffers, this->frames(), this->soundfile.data);
        else
          return sample_generic(
              frames_to_read, numchannels, read_buffers, this->frames(), this->channels(),
              this->soundfile.data);
      default:
        return sample_generic(
            frames_to_read, numchannels, read_buffers, this->frames(), this->channels(),
            this->soundfile.data);
    }
  }


  void sample(int frames_to_write, int numchannels, double** buffers, double pitch, double vol)
  {
    if(this->voices.empty())
      return;
    if(this->channels() == 0)
      return;
    if(frames_to_write == 0 || numchannels == 0)
      return;

    fetch_audio(numchannels, frames_to_write, buffers);

    for(int c = 0; c < numchannels; ++c)
    {
      for(int f = 0; f < frames_to_write; f++)
      {
        buffers[c][f] *= vol;
      }
    }
  }

  void sample_pitched(int frames_to_write, int numchannels, double** buffers, double pitch, double vol)
  {
    if(this->voices.empty())
      return;
    if(this->channels() == 0)
      return;
    if(frames_to_write == 0 || numchannels == 0)
      return;

    assert(this->m_rubberBand);
    if(pitch != this->m_rubberBand->getPitchScale())
      this->m_rubberBand->setPitchScale(pitch);

    int frames_to_read = std::max((int)this->m_rubberBand->getSamplesRequired(), (int) (double(frames_to_write) * pitch));
    if(frames_to_read < 16)
      frames_to_read = 16;

    float* read_buffer = (float*)alloca(numchannels * frames_to_read * sizeof(float));
    std::fill_n(read_buffer, numchannels * frames_to_read, 0.f);
    float** read_buffers = (float**)alloca(numchannels * sizeof(float*));
    for(int i = 0; i < numchannels; i++)
      read_buffers[i] = read_buffer + i * frames_to_read;

    float* src_buffer = (float*)alloca(numchannels * frames_to_write * sizeof(float));
    std::fill_n(src_buffer, numchannels * frames_to_write, 0.f);
    float** src_buffers = (float**)alloca(numchannels * sizeof(float*));
    for(int i = 0; i < numchannels; i++)
      src_buffers[i] = src_buffer + i * frames_to_write;

    while(m_rubberBand->available() < frames_to_write)
    {
      fetch_audio(numchannels, frames_to_read, read_buffers);
      this->m_rubberBand->process(read_buffers, frames_to_read, false);
      frames_to_read = 16;
    }

    m_rubberBand->retrieve(src_buffers, frames_to_write);

    for(int c = 0; c < numchannels; ++c)
    {
      for(int f = 0; f < frames_to_write; f++)
      {
        buffers[c][f] = src_buffers[c][f] * vol;
      }
    }
  }

  RubberBand::RubberBandStretcher* m_rubberBand{};
};

}
