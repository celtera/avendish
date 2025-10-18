#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

namespace examples
{

struct SampleAccurateGeneratorExample
{
  halp_meta(name, "My sample-accurate generator");
  halp_meta(c_name, "oscr_SampleAccurateGeneratorExample")
  halp_meta(category, "Demo");
  halp_meta(author, "<AUTHOR>");
  halp_meta(description, "<DESCRIPTION>");
  halp_meta(uuid, "c519b3c4-326e-4e80-8dec-d465264c5b08");

  /**
   * Here we define a single output, which allows writing
   * sample-accurate data to an output port of the node.
   * Timestamps start from zero (at the beginning of a buffer) to N:
   * i ∈ [0; N( in the usual mathematic notation.
   */
  struct
  {
    halp::accurate<halp::val_port<"Out", int>> value;
  } outputs;

  /**
   * Called at least once per cycle.
   * Note that buffer sizes aren't necessarily powers-of-two: N can be 1 for instance.
   *
   * The output buffer is guaranteed to be empty before the function is called.
   */
  void operator()(std::size_t N)
  {
    // 0 : first sample of the buffer.
    outputs.value.values[0] = std::rand() % 100;
  }
};
}
