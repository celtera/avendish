#pragma once
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <halp/sound_set.hpp>
#include <rnd/random.hpp>

#include <vector>

// The host lists, decodes and watches the folder: see halp::sound_set.

namespace ao
{
class AudioParticles
{
public:
  halp_meta(name, "Audio particles")
  halp_meta(category, "Audio/Generators")
  halp_meta(c_name, "avnd_audio_particles")
  halp_meta(description, "Trigger overlapping sounds from a folder across multiple audio channels with controllable timing and density.")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/audio-particles.html")
  halp_meta(uuid, "e7f2b091-0de0-49cd-a581-d4087d901fbb")

  struct ins
  {
    struct : halp::sound_set<"Folder">
    {
      static constexpr bool recursive() { return true; }
    } folder;
    halp::spinbox_i32<"Channels", halp::range{.min = 0., .max = 128, .init = 16}>
        channels;
    struct : halp::time_chooser<"Frequency", halp::range{0.00001, 30., 0.2}>
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

  //! A sound playing: it keeps its file alive until it ends, even if the folder
  //! changes meanwhile.
  struct Playhead
  {
    const float* const* data{};
    int64_t frames{};
    int64_t frame{};
    int channels{};
    uint16_t channel{};
    halp::sound_pin pin;
  };

  //! Sounds playing at once: reserved in prepare(), so that a new one never
  //! allocates on the audio thread.
  static constexpr std::size_t max_playheads = 1024;
  std::vector<Playhead> m_playheads;
  double rate{};
  std::random_device m_rdev;
  rnd::pcg m_rng{m_rdev};
};

}
