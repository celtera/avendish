#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>

namespace examples
{

struct TrivialGeneratorExample
{
  $(name, "My trivial generator");
  $(script_name, "trivial_effect_gen");
  $(category, "Demo");
  $(author, "<AUTHOR>");
  $(description, "<DESCRIPTION>");
  $(uuid, "29099d09-cbd1-451b-8394-972b0d5bfaf0");

  /**
   * Here we define a single output, which allows writing
   * a single output value every time the process is run.
   */
  struct
  {
    struct
    {
      // Give a name to our parameter to show the user
      $(name, "Out");

      // This value will be sent to the output of the port at each tick.
      // The name "value" is important.
      int value{};
    } main;  // This variable can be called however you wish.
  } outputs; // This must be called "outputs".

  /**
   * Called at least once per cycle.
   * The value will be stored at the start of the tick.
   */
  void operator()()
  {
    outputs.main.value++;
    if (outputs.main.value > 100)
      outputs.main.value = 0;
  }
};

}
