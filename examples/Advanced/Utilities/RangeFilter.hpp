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
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/mapping-utilities.html#range-filter")
  halp_meta(uuid, "db16b5fa-e6b0-4f89-8210-225384dbc677")
  halp_flag(cv);
  halp_flag(stateless);

  struct inputs_t
  {
    halp::spinbox_f32<"Min", halp::range{-1e6, 1e6, 0.}> min;
    halp::spinbox_f32<"Max", halp::range{-1e6, 1e6, 1.}> max;
    struct : halp::toggle<"Invert">
    {
      halp_meta(
          description,
          "Invert the filter operation: only let values outside the range go through.")
    } invert;
  } inputs;

  struct
  {
  } outputs;

  std::optional<float> operator()(float v) noexcept
  {
    if(inputs.invert)
    {
      if(v <= inputs.min || v >= inputs.max)
      {
        return v;
      }
    }
    else
    {
      if(v >= inputs.min && v <= inputs.max)
      {
        return v;
      }
    }
    return std::nullopt;
  }
};
}
