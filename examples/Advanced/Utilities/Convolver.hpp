#pragma once
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <kfr/dft.hpp>
#include <kfr/dft/convolution.hpp>
#include <kfr/kfr.h>

namespace ao
{
/**
 * @brief Multi-channel convolver based on kfr
 */
struct Convolver
{
public:
  halp_meta(name, "Convolver")
  halp_meta(c_name, "kfr_convolver")
  halp_meta(author, "KFRLib")
  halp_meta(category, "Audio/Effects")
  halp_meta(description, "Impulse response convolver")
  halp_meta(uuid, "5bd72341-20a5-4b79-b0f3-6586be334610")

  struct
  {
    halp::dynamic_audio_bus<"In", double> audio;
    struct : halp::soundfile_port<"Impulse">
    {
      void update(Convolver& conv)
      {
        if(this->soundfile.channels > 0)
        {
          conv.impulse_response.assign(
              this->soundfile.data[0], this->soundfile.data[0] + this->soundfile.frames);

          conv.recreate();
        }
        else
        {
          conv.reverb.clear();
        }
      }
    } ir;
  } inputs;

  struct outputs
  {
    halp::dynamic_audio_bus<"Out", double> audio;
  } outputs;

  void recreate()
  {
    int in_channels = inputs.audio.channels;
    if(impulse_response.size() == 0 || s.frames == 0)
    {
      reverb.clear();
      return;
    }

    for(int i = 0; i < std::ssize(reverb); i++)
    {
      reverb[i]->set_data(impulse_response);
    }

    for(int i = reverb.size(); i < in_channels; i++)
    {
      reverb.emplace_back(
          std::make_unique<kfr::convolve_filter<float>>(impulse_response, s.frames));
    }
  }

  halp::setup s;
  void prepare(halp::setup info) noexcept
  {
    s = info;
    recreate();
  }

  void operator()(int frames) noexcept
  {
    if(frames != s.frames)
      return;
    using namespace kfr;
    if(std::ssize(reverb) < inputs.audio.channels)
      recreate();

    if(std::ssize(reverb) < inputs.audio.channels)
      return;

    for(int i = 0; i < inputs.audio.channels; i++)
    {
      audio.assign(inputs.audio.samples[i], inputs.audio.samples[i] + frames);
      reverb[i]->apply(audio);
      std::copy_n(audio.data(), frames, outputs.audio.samples[i]);
    }
  }

  kfr::univector<float> impulse_response;
  kfr::univector<float> audio;
  boost::container::small_vector<std::unique_ptr<kfr::convolve_filter<float>>, 2> reverb;
};

}
