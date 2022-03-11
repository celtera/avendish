#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/log.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>

namespace examples
{
/**
 * This example is the simplest possible score process. It will just print a hello world
 * message repeatedly when executed.
 */
struct EmptyExample
{
  /** Here are the metadata of the plug-ins, to display to the user **/
  $(name, "Hello world");
  $(script_name, "empty_example");
  $(category, "Demo");
  $(author, "<AUTHOR>");
  $(description, "<DESCRIPTION>");
  $(uuid, "1c0f91ee-52da-4a49-a70a-4530a24b152b");

  /** This function will be called repeatedly at the tick rate of the environment **/
  void operator()()
  {
    avnd::basic_logger{}.log("henlo");
  }
};
}
