#pragma once
#include <Media/AudioDecoder.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <rnd/random.hpp>

#include <QFile>
namespace ao
{
class AudioParticles
{
public:
  halp_meta(name, "Audio particles")
  halp_meta(category, "Audio/Generator")
  halp_meta(c_name, "avnd_audio_particles")
  halp_meta(uuid, "e7f2b091-0de0-49cd-a581-d4087d901fbb")

  struct ins
  {
    halp::lineedit<"Folder", ""> folder;
    halp::spinbox_i32<"Channels", halp::range{.min = 0., .max = 128, .init = 16}>
        channels;
    struct : halp::time_chooser<"Frequency", halp::range{0.001, 30., 0.2}>
    {
      using mapper = halp::log_mapper<std::ratio<95, 100>>;
    } frequency;
    halp::knob_f32<"Density", halp::range{0.001, 1., 0.7}> density;
  } inputs;

  struct
  {
    halp::variable_audio_bus<"Output", double> audio;
  } outputs;

  using setup = halp::setup;
  void prepare(halp::setup info);

  // Do our processing for N samples
  using tick = halp::tick_musical;

  // Defined in the .cpp
  void operator()(const halp::tick_musical& t);

  std::vector<Media::audio_array> m_sounds;

  struct Playhead
  {
    int frame{};
    uint16_t index{};
    uint16_t channel{};
  };

  std::vector<Playhead> m_playheads;
  double rate{};
  std::random_device m_rdev;
  rnd::pcg m_rng{m_rdev};
};

}
