#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

#include <optional>

namespace ao
{
/**
 * @brief Repears the last value
 */
struct Repeat
{
  halp_meta(name, "Repeat")
  halp_meta(c_name, "repeat")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Continuously outputs the last non-null input value")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/mapping-utilities.html#repeat")
  halp_meta(uuid, "61cc60a7-5b32-40e9-9149-fe8b259f07be")

  struct
  {
    halp::val_port<"Input", std::optional<ossia::value>> input;
  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> output;
  } outputs;

  ossia::value previous_value;

  void operator()()
  {
    if(inputs.input.value)
      previous_value = *inputs.input.value;

    if(previous_value.valid())
      outputs.output.value = previous_value;
  }
};
}
