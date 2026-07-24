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
struct Spigot
{
  halp_meta(name, "Spigot")
  halp_meta(c_name, "Spigot")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Outputs the input value only if the toggle is on")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/mapping-utilities.html#spigot")
  halp_meta(uuid, "8b75d69b-5ce4-4360-a066-c4a7f37f3353")

  struct
  {
    halp::val_port<"Input", std::optional<ossia::value>> input;
    halp::toggle<"Enabled"> enabled;
  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> output;
  } outputs;

  void operator()()
  {
    if(inputs.enabled && inputs.input.value)
      outputs.output.value = *inputs.input.value;
  }
};
}
