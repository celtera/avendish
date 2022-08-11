#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

namespace examples
{
struct SampleAccurateFilterExample
{
  halp_meta(name, "My sample-accurate filter");
  halp_meta(c_name, "sample_acc_filt");
  halp_meta(category, "Demo");
  halp_meta(author, "<AUTHOR>");
  halp_meta(description, "<DESCRIPTION>");
  halp_meta(uuid, "43818edd-63de-458b-a6a5-08033cefc051");

  /**
   * Here we define an input and an output pair.
   */
  struct
  {
    halp::accurate<halp::val_port<"In", float>> value;
  } inputs;

  struct
  {
    halp::accurate<halp::val_port<"Out", float>> value;
  } outputs;

  void operator()()
  {
    // The output is copied at the same timestamp at which each input happened.
    for(auto& [timestamp, value] : inputs.value.values)
    {
      outputs.value.values[timestamp] = cos(value * value) * cos(value);
    }
  }
};
}
