#include "AudioParticles.hpp"

#include <ossia/detail/algorithms.hpp>
#include <score/tools/Debug.hpp>
#include <score/tools/RecursiveWatch.hpp>
#include <score/tools/Debug.hpp>
#include <ossia/detail/algorithms.hpp>
#include <QDirIterator>
namespace ao
{
void AudioParticles::prepare(halp::setup info)
{
  this->rate = info.rate;
  // Initialization, this method will be called with buffer size, etc.
  m_sounds.clear();
  m_sounds.reserve(1000);

  // FIXME move to worker thread
  score::for_all_files(inputs.folder.value, [this](std::string_view v) {
    if(!v.ends_with(".wav"))
      return;

    const QString& path = QString::fromUtf8(v.data(), v.size());

    auto dec = Media::AudioDecoder::decode_synchronous(path, this->rate);
    if(!dec)
      return;

    m_sounds.push_back(std::move(dec->second));
  });
}

std::optional<int>
frame_in_interval(int start_frame, int end_frame, double rate, double freq)
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

  if(m_sounds.empty())
    return;

  // FIXME range is not respected

  if(inputs.frequency > 0.)
  {
    // Trigger new sounds
    bool is_in_frame = false;
    if(inputs.frequency.sync)
    {
      is_in_frame = !t.get_quantification_date(1. / inputs.frequency.value).empty();
    }
    else
    {
      is_in_frame = bool(frame_in_interval(
          t.position_in_frames, t.position_in_frames + t.frames, rate,
          inputs.frequency));
    }
    if(is_in_frame)
    {
      if((1. - inputs.density) < std::exponential_distribution<float>()(m_rng))
        m_playheads.push_back(
            Playhead{
                0, uint16_t(unsigned(m_rng()) % m_sounds.size()),
                uint16_t(unsigned(m_rng()) % this->outputs.audio.channels)});
    }
  }

  for(auto& playhead : m_playheads)
  {
    if(m_sounds.size() <= playhead.index)
      continue;

    auto& sound = m_sounds[playhead.index];
    int sound_frames = sound[0].size();

    if(outputs.audio.channels <= playhead.channel)
      continue;
    auto channel = outputs.audio.channel(playhead.channel, t.frames);

    if(sound_frames - playhead.frame > t.frames)
      for(int i = playhead.frame; i < playhead.frame + t.frames; i++)
        channel[i - playhead.frame] += sound[0][i];
    else
    {
      for(int i = playhead.frame; i < sound_frames; i++)
        channel[i - playhead.frame] += sound[0][i];
    }
    playhead.frame += t.frames;
  }

  ossia::remove_erase_if(m_playheads, [this](const auto& playhead) {
    if(outputs.audio.channels <= playhead.channel)
      return true;
    if(std::ssize(m_sounds) <= playhead.index)
      return true;

    auto& sound = m_sounds[playhead.index];
    SCORE_ASSERT(!sound.empty());
    return playhead.frame >= std::ssize(sound[0]);
  });
}
}
