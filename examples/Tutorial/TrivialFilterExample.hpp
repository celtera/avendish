#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <cmath>

namespace examples
{

struct TrivialFilterExample
{
  halp_meta(name, "My trivial filter");
  halp_meta(c_name, "trivial_effect_filter");
  halp_meta(category, "Demo");
  halp_meta(author, "<AUTHOR>");
  halp_meta(description, "<DESCRIPTION>");
  halp_meta(uuid, "d02006f0-3e71-465b-989c-7c53aaa885e5");

  /**
   * Here we define a pair of input / outputs.
   */
  struct
  {
    struct
    {
      // Give a name to our parameter to show the user
      halp_meta(name, "In");

      float value{};
    } main; // Name does not matter
  } inputs; // Inputs have to be inside an "input" member.

  struct
  {
    struct
    {
      halp_meta(name, "Out");

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
