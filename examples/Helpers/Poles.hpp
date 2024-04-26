#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls_fmt.hpp>
#include <halp/log.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>

namespace examples::helpers
{
/**
 * List of 0-1 values with multiple poles
 */
struct Poles
{
  halp_meta(name, "Poles")
  halp_meta(c_name, "avnd_poles")
  halp_meta(category, "Control/Generators")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Generate a gaussian pole")
  halp_meta(uuid, "c01c06ed-2005-46d5-a47b-d9dbc947afea")
  struct
  {
    struct : halp::spinbox_i32<"Length", halp::range{1, 1000, 20}>
    {
      void update(Poles& self)
      {
        if(value >= 0 && value < 1000)
          self.outputs.a.value.reserve(value);
      }
    } length;

    halp::knob_f32<"Pos", halp::range{-1., 1., 0.}> pos;

    struct : halp::knob_f32<"Sigma", halp::range{0.001, 1., 0.01}>
    {
      using mapper = halp::log_mapper<std::ratio<95, 100>>;
      void update(Poles& self)
      {
        if(value > 0)
        {
          self.inv_sigma = 1. / value;
        }
      }
    } sigma;
  } inputs;

  struct
  {
    halp::val_port<"Out", std::vector<float>> a;
  } outputs;

  static double pdf(double x, double inv_sigma) noexcept
  {
    return 0.3989422804014327 * inv_sigma
           * std::exp(-0.5 * x * inv_sigma * x * inv_sigma);
  }

  void operator()()
  {
    if(inputs.length.value < 0 || inputs.length.value > 1000)
      return;

    std::vector<float>& res = outputs.a.value;
    res.resize(inputs.length + 1);

    std::string s;
    for(int k = 0; k <= inputs.length; k++)
    {
      const double i = 2. * k / inputs.length - 1.;
      res[k] = pdf(i / 10. - inputs.pos, inv_sigma);
    }
  }
  double inv_sigma{1.};
};
}
