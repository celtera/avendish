#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>

#include <cmath>

namespace examples
{

struct TrivialFilterExample
{
  $(name, "My trivial filter");
  $(script_name, "trivial_effect_filter");
  $(category, "Demo");
  $(author, "<AUTHOR>");
  $(description, "<DESCRIPTION>");
  $(uuid, "d02006f0-3e71-465b-989c-7c53aaa885e5");

  /**
   * Here we define a pair of input / outputs.
   */
  struct {
    struct {
      // Give a name to our parameter to show the user
      $(name, "In");

      float value{};
    } main; // Name does not matter
  } inputs; // Inputs have to be inside an "input" member.

  struct {
    struct {
      $(name, "Out");

      int value{};
    } main;
  } outputs;

  /**
   * Called at least once per cycle.
   * The value will be stored at the start of the tick.
   */
  void operator()()
  {
    outputs.main.value = inputs.main.value > 0 ? std::log(inputs.main.value) : 0;
  }
};

}
