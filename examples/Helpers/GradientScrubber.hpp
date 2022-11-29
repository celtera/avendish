#pragma once
/**
 * Note: this example uses libossia in order to not have to reimplement
 * its extensive color dataspace conversion and easings library
 */
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <ossia/network/dataspace/color.hpp>
#include <ossia/network/dataspace/dataspace.hpp>

namespace examples::helpers
{
struct GradientScrub
{
  halp_meta(name, "Gradient scrubber")
  halp_meta(c_name, "avnd_gradient_scrub")
  halp_meta(category, "Control/Color")
  halp_meta(uuid, "27da6372-a349-4b32-a030-23e6e0a6ab59")

  struct
  {
    struct : halp::color_chooser<"Start">
    {
      void update(GradientScrub& self)
      {
        // Convert to La*b*
        self.start_lab = ossia::hunter_lab{ossia::rgb{value.r, value.g, value.b}};
      }
    } c0;
    struct : halp::color_chooser<"End">
    {
      void update(GradientScrub& self)
      {
        // Convert to La*b*
        self.end_lab = ossia::hunter_lab{ossia::rgb{value.r, value.g, value.b}};
      }
    } c1;

    halp::hslider_f32<"Pixel 1"> pix0;
    halp::hslider_f32<"Pixel 2"> pix1;
    halp::hslider_f32<"Pixel 3"> pix2;
    halp::hslider_f32<"Pixel 4"> pix3;
    halp::hslider_f32<"Pixel 5"> pix4;
    halp::hslider_f32<"Pixel 6"> pix5;
    halp::hslider_f32<"Pixel 7"> pix6;
    halp::hslider_f32<"Pixel 8"> pix7;
    halp::hslider_f32<"Power 1", halp::range{0.1, 10., 2.}> pow0;
    halp::hslider_f32<"Power 2", halp::range{0.1, 10., 2.}> pow1;
    halp::hslider_f32<"Power 3", halp::range{0.1, 10., 2.}> pow2;
    halp::hslider_f32<"Power 4", halp::range{0.1, 10., 2.}> pow3;
    halp::hslider_f32<"Power 5", halp::range{0.1, 10., 2.}> pow4;
    halp::hslider_f32<"Power 6", halp::range{0.1, 10., 2.}> pow5;
    halp::hslider_f32<"Power 7", halp::range{0.1, 10., 2.}> pow6;
    halp::hslider_f32<"Power 8", halp::range{0.1, 10., 2.}> pow7;
  } inputs;

  struct
  {
    halp::val_port<"Out 1", halp::color_type> o0;
    halp::val_port<"Out 2", halp::color_type> o1;
    halp::val_port<"Out 3", halp::color_type> o2;
    halp::val_port<"Out 4", halp::color_type> o3;
    halp::val_port<"Out 5", halp::color_type> o4;
    halp::val_port<"Out 6", halp::color_type> o5;
    halp::val_port<"Out 7", halp::color_type> o6;
    halp::val_port<"Out 8", halp::color_type> o7;
  } outputs;

  // clang-format off
  halp::color_type interpolate(float percentage, float power)
  {
    static constexpr const auto easer = ossia::easing::ease{};

    const auto eased = std::pow(percentage, power);
    const auto start = start_lab.dataspace_value;
    const auto end = end_lab.dataspace_value;

    // Ease in the La*b* domain
    ossia::hunter_lab res = ossia::vec3f{
        easer(start[0], end[0], eased),
        easer(start[1], end[1], eased),
        easer(start[2], end[2], eased)};

    // Back to rgb
    ossia::rgb col = res;
    return {col.dataspace_value[0], col.dataspace_value[1], col.dataspace_value[2], 1.0};
  }
  // clang-format on

  ossia::hunter_lab start_lab;
  ossia::hunter_lab end_lab;
  void operator()()
  {
    outputs.o0 = interpolate(inputs.pix0, inputs.pow0);
    outputs.o1 = interpolate(inputs.pix1, inputs.pow1);
    outputs.o2 = interpolate(inputs.pix2, inputs.pow2);
    outputs.o3 = interpolate(inputs.pix3, inputs.pow3);
    outputs.o4 = interpolate(inputs.pix4, inputs.pow4);
    outputs.o5 = interpolate(inputs.pix5, inputs.pow5);
    outputs.o6 = interpolate(inputs.pix6, inputs.pow6);
    outputs.o7 = interpolate(inputs.pix7, inputs.pow7);
  }
};
}
