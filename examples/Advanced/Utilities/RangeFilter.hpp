#pragma once

#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/math.hpp>

namespace ao
{
/**
 * @brief Filter undesired values of a sensor
 */
struct RangeFilter
{
public:
  halp_meta(name, "Range Filter")
  halp_meta(c_name, "range_filter")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Filter out values of an input outside of a numeric range")
  halp_meta(uuid, "db16b5fa-e6b0-4f89-8210-225384dbc677")

  struct inputs_t
  {
    halp::val_port<"In", std::optional<float>> in;
    halp::spinbox_f32<"Min", halp::range{-1e6, 1e6, 0.}> min;
    halp::spinbox_f32<"Max", halp::range{-1e6, 1e6, 1.}> max;
  } inputs;

  struct
  {
    halp::val_port<"Out", std::optional<float>> out;
  } outputs;

  void operator()() noexcept
  {
    if(inputs.in.value)
    {
      float v = *inputs.in.value;
      if(v >= inputs.min && v <= inputs.max)
        outputs.out = v;
    }
  }
};

}
