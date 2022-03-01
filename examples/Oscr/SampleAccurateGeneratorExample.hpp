#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>

namespace examples
{

struct SampleAccurateGeneratorExample
{
  $(pretty_name, "My sample-accurate generator");
  $(script_name, "sample_acc_gen");
  $(category, "Demo");
  $(author, "<AUTHOR>");
  $(description, "<DESCRIPTION>");
  $(uuid, "c519b3c4-326e-4e80-8dec-d465264c5b08");

  /**
   * Here we define a single output, which allows writing
   * sample-accurate data to an output port of the node.
   * Timestamps start from zero (at the beginning of a buffer) to N:
   * i ∈ [0; N( in the usual mathematic notation.
   */
  struct {
    avnd::accurate<avnd::val_port<"Out", int>> value;
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
