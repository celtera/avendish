#pragma once

#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/math.hpp>

namespace ao
{

struct RangeFilter
{
public:
  halp_meta(name, "Range Mapper")
  halp_meta(c_name, "range_mapper")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Map values in a numeric range")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/mapping-utilities.html#range-mapper")
  halp_meta(uuid, "cb6f19b2-827d-420c-ad19-7a366f914026")

  struct inputs_t
  {
    halp::val_port<"In", float> in;
    halp::spinbox_f32<"In min", halp::range{-1e6, 1e6, 0.}> imin;
    halp::spinbox_f32<"In max", halp::range{-1e6, 1e6, 1.}> imax;
    halp::spinbox_f32<"Out min", halp::range{-1e6, 1e6, 0.}> omin;
    halp::spinbox_f32<"Out max", halp::range{-1e6, 1e6, 1.}> omax;
  } inputs;

  struct
  {
    halp::val_port<"Out", float> out;
  } outputs;

  template <typename T>
  static T
  scale(T value, double src_min, double src_max, double dst_min, double dst_max) noexcept
  {
    const double sub = src_max - src_min;
    if(sub == 0.)
      return value;

    auto ratio = (dst_max - dst_min) / sub;
    value = dst_min + ratio * (value - src_min);
    return value;
  }

  void operator()() noexcept
  {
    outputs.out = scale(inputs.in, inputs.imin, inputs.imax, inputs.omin, inputs.omax);
  }
};

}
