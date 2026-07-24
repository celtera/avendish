#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

#include <optional>

namespace ao
{
/**
 * @brief Flip flop flip flop flip flop
 */
struct FlipFlop
{
  halp_meta(name, "Flip Flop")
  halp_meta(c_name, "flipflop")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Outputs true / false alternatively on each trigger")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/mapping-utilities.html#flipflop")
  halp_meta(uuid, "a46b878e-ee02-422c-8a77-3e883b247867")

  struct
  {
    halp::val_port<"Input", std::optional<ossia::value>> input;
  } inputs;

  struct
  {
    halp::val_port<"Output", bool> output;
  } outputs;

  bool last = false;

  void operator()()
  {
    if(inputs.input.value)
      last = !last;
    outputs.output.value = last;
  }
};
}
