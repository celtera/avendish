#pragma once

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>

namespace ao
{
/**
 * @brief Simple bitcrush based on Lance Putnam's Gamma library
 */
struct ControlToCV
{
public:
  halp_meta(name, "Control to CV")
  halp_meta(c_name, "control_to_cv")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Convert control values to CV")
  halp_meta(category, "Audio")
  halp_meta(uuid, "22076692-8ac2-43e4-bfca-0bf7945a748b")

  struct
  {
    halp::val_port<"Input", float> value;
  } inputs;

  struct
  {
    halp::audio_sample<"Output", float> audio;
  } outputs;

  float interpolation{};

  void operator()() noexcept { outputs.audio = inputs.value; }
};

}
