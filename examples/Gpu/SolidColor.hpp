#pragma once
#include <gpp/commands.hpp>
#include <gpp/layout.hpp>
#include <gpp/meta.hpp>
#include <gpp/ports.hpp>
#include <halp/controls.hpp>

namespace examples
{
struct GpuSolidColorExample
{
  halp_meta(name, "Solid color");
  halp_meta(uuid, "c9a5fd8e-b66e-41ff-8feb-bca2cdab4990");

  struct layout
  {
    enum
    {
      graphics
    };

    // Here we only need a fragment output
    struct fragment_output
    {
      gpp_attribute(0, fragColor, float[4])
      fragColor;
    } fragment_output;

    // We define one UBO with a vec4: our solid color
    struct bindings
    {
      struct custom_ubo
      {
        halp_meta(name, "custom");
        halp_meta(binding, 0);
        halp_flags(std140, ubo);

        gpp::uniform<"color", float[4]> col;
      } ubo;
    };
  };

  struct
  {
    // Make this uniform visible as a port
    gpp::uniform_control_port<
        halp::color_chooser<"Color">, &layout::bindings::custom_ubo::col>
        color;
  } inputs;

  struct
  {
    // Make an output port from the fragment output
    gpp::color_attachment_port<"Color output", &layout::fragment_output> out;
  } outputs;

  // Trivial fragment shader
  std::string_view fragment()
  {
    return R"_(
void main() {
  fragColor = color;
}
)_";
  }
};
}
