#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

#include <optional>

namespace ao
{
/**
 * @brief Filter repetitive values - only output when input differs from previous value
 */
struct RepetitionFilter
{
  halp_meta(name, "Repetition Filter")
  halp_meta(c_name, "repetition_filter")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Only output values that are different from the previous input")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/mapping-utilities.html#repetition-filter")
  halp_meta(uuid, "120a50ad-4e74-43c4-a05b-3e645b27e152")

  struct
  {
    halp::val_port<"Input", ossia::value> input;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::optional<ossia::value>> output;
  } outputs;

  std::optional<ossia::value> previous_value;

  void operator()()
  {
    if(!previous_value || inputs.input.value != *previous_value)
    {
      outputs.output.value = inputs.input.value;
      previous_value = inputs.input.value;
    }
    else
    {
      outputs.output.value = std::nullopt;
    }
  }
};
}
