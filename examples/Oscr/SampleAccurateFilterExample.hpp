#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>

#include <avnd/concepts/parameter.hpp>
#include <cmath>

namespace oscr
{
}

namespace examples
{

struct SampleAccurateFilterExample
{
  $(name, "My sample-accurate filter");
  $(script_name, "sample_acc_filt");
  $(category, "Demo");
  $(author, "<AUTHOR>");
  $(description, "<DESCRIPTION>");
  $(uuid, "43818edd-63de-458b-a6a5-08033cefc051");

  /**
   * Here we define an input and an output pair.
   */
  struct {
    avnd::accurate<avnd::val_port<"In", float>> value;
  } inputs;

  struct {
    avnd::accurate<avnd::val_port<"Out", float>> value;
  } outputs;

  void operator()()
  {
    // The output is copied at the same timestamp at which each input happened.
    for(auto& [timestamp, value]: inputs.value.values)
    {
      outputs.value.values[timestamp] = cos(value * value) * cos(value);
    }
  }
};
}
