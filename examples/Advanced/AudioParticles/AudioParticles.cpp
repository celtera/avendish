#include "AudioParticles.hpp"

#include <ossia/detail/algorithms.hpp>

#include <algorithm>
namespace ao
{
void AudioParticles::prepare(halp::setup info)
{
  this->rate = info.rate;
  m_playheads.reserve(max_playheads);
}

std::optional<int>
frame_in_interval(int64_t start_frame, int64_t end_frame, double rate, double freq)
{
  if(rate <= 0.)
    return std::nullopt;

  int64_t start_ns = 1e9 * start_frame / rate;
  int64_t end_ns = 1e9 * end_frame / rate;
  int64_t itv_ns = 1e9 / freq;
  auto start_div = std::div(start_ns, itv_ns);
  auto end_div = std::div(end_ns, itv_ns);

  if(end_div.quot > start_div.quot)
  {
    return 1;
  }
  return std::nullopt;
}
void AudioParticles::operator()(const halp::tick_musical& t)
{
  if(this->outputs.audio.channels != this->inputs.channels.value)
  {
    this->outputs.audio.request_channels(this->inputs.channels.value);
    return;
  }

  // FIXME range is not respected

  if(inputs.frequency > 0.)
  {
    // Trigger new sounds
    bool is_in_frame = false;
    if(inputs.frequency.sync)
    {
      // The binding hands a synced value over in seconds at the current tempo;
      // the grid wants its rate (1 a whole note, 4 a quarter...).
      const double secs = inputs.frequency.value;
      if(secs > 0. && t.tempo > 0.)
        is_in_frame = !t.get_quantification_date(240. / (secs * t.tempo)).empty();
    }
    else
    {
      is_in_frame = bool(frame_in_interval(
          t.position_in_frames, t.position_in_frames + t.frames, rate,
          inputs.frequency));
    }
    const auto& sounds = inputs.folder;
    if(is_in_frame && m_playheads.size() < max_playheads && !sounds.empty()
       && this->outputs.audio.channels > 0)
    {
      if((1. - inputs.density) < std::exponential_distribution<float>()(m_rng))
      {
        const auto index = unsigned(m_rng()) % sounds.size();
        if(const auto& sound = sounds[index];
           sound.status == halp::sound_status::ready && sound.frames > 0)
        {
          m_playheads.push_back(Playhead{
              .data = sound.data,
              .frames = sound.frames,
              .frame = 0,
              .channels = sound.channels,
              .channel = uint16_t(unsigned(m_rng()) % this->outputs.audio.channels),
              .pin = sounds.pin(index)});
        }
      }
    }
  }

  for(auto& playhead : m_playheads)
  {
    if(outputs.audio.channels <= playhead.channel)
      continue;
    auto channel = outputs.audio.channel(playhead.channel, t.frames);

    // Every channel of the file, mixed down to the one it plays on
    const int64_t n = std::min<int64_t>(t.frames, playhead.frames - playhead.frame);
    const double gain = 1. / playhead.channels;
    for(int c = 0; c < playhead.channels; c++)
    {
      const float* src = playhead.data[c] + playhead.frame;
      for(int64_t i = 0; i < n; i++)
        channel[i] += gain * src[i];
    }
    playhead.frame += t.frames;
  }

  ossia::remove_erase_if(m_playheads, [this](const auto& playhead) {
    return outputs.audio.channels <= playhead.channel
           || playhead.frame >= playhead.frames;
  });
}
}
