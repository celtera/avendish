#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/log.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

namespace examples
{
/**
 * This example is the simplest possible score process. It will just print a hello world
 * message repeatedly when executed.
 */
struct EmptyExample
{
  /** Here are the metadata of the plug-ins, to display to the user **/
  halp_meta(name, "Hello world");
  halp_meta(c_name, "empty_example");
  halp_meta(category, "Demo");
  halp_meta(author, "<AUTHOR>");
  halp_meta(description, "<DESCRIPTION>");
  halp_meta(uuid, "1c0f91ee-52da-4a49-a70a-4530a24b152b");

  /** This function will be called repeatedly at the tick rate of the environment **/
  void operator()() { halp::basic_logger{}.info("henlo"); }
};
}
