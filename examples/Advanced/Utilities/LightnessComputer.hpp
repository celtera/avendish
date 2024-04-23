#pragma once

#include <boost/functional/hash.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <chrono>
namespace vo
{
// Graphical item which will display the texture
struct LightnessComputerTextureDisplay
{
  using item_type = LightnessComputerTextureDisplay;
  static constexpr double width() { return 64.; }
  static constexpr double height() { return 64.; }

  void paint(auto ctx)
  {
    if(m_w <= 0 || m_h <= 0)
      return;

    // Render the texture
    ctx.draw_bytes(0, 0, width(), height(), m_bytes.data(), m_w, m_h);

    ctx.update();
  }

  void update(std::vector<unsigned char>&& bytes, int w, int h)
  {
    m_bytes = std::move(bytes);
    m_w = w;
    m_h = h;
  }

  std::vector<unsigned char> m_bytes = std::vector<unsigned char>(200 * 200 * 4);
  int m_w{0};
  int m_h{0};
};

// Main processor object
struct LightnessComputer
{
public:
  halp_meta(name, "Lightness computer");
  halp_meta(c_name, "lightness_computer");
  halp_meta(category, "Visuals/Computer Vision");
  halp_meta(author, "Jean-Michaël Celerier");
  halp_meta(description, "Convert an image to a list of lightness values.");
  halp_meta(uuid, "60a11c39-dc14-449d-bbe4-1c51e44ea99a");

  struct ins
  {
    halp::fixed_texture_input<"In"> image{.request_width = 16, .request_height = 16};
    halp::xy_spinboxes_i32<"Size", halp::range{1, 64, 10}> size;
  } inputs;

  struct
  {
    halp::val_port<"Samples", std::vector<float>> samples;
  } outputs;

  // Some utility functions to extract lightness
  static constexpr double linear_to_y(float r, float g, float b) noexcept
  {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
  }

  static constexpr double y_to_lstar(float y) noexcept
  {
    if(y <= 216.f / 24389.f)
      return y * (24.389f / 27.f);
    else
      return 0.01f * (std::cbrt(y) * 116.f - 16.f);
  }

  // Communication with UI
  struct processor_to_ui
  {
    halp_flag(relocatable);

    std::vector<unsigned char> bytes;
    int w, h;
  };
  std::function<void(processor_to_ui)> send_message;

  using clk = std::chrono::steady_clock;
  clk::time_point last_ui_message = clk::now();

  void init()
  {
    inputs.image.request_width = inputs.size.value.x;
    inputs.image.request_height = inputs.size.value.y;
  }

  // Main processing
  void operator()()
  {
    auto& in_tex = inputs.image.texture;
    if(!in_tex.changed)
    {
      return;
    }

    if(!in_tex.bytes || in_tex.width < 1 || in_tex.height < 1)
    {
      return;
    }

    // Sample the texture
    auto& samples = outputs.samples.value;
    samples.clear();
    samples.resize(in_tex.height * in_tex.width);
    int i = 0;
#pragma omp simd
    for(int y = 0; y < in_tex.height; y++)
    {
      auto row = inputs.image.row(y);
      for(int x = 0; x < in_tex.width; x++)
      {
        const auto [r, g, b, a] = inputs.image.get(x, row);
        const auto lightness = linear_to_y(r / 255.f, g / 255.f, b / 255.f);
        const auto perceptual_lightness = y_to_lstar(lightness);
        samples[i] = perceptual_lightness;
        i++;
      }
    }

    // Notify the UI with the new texture at a reduced rate
    if(send_message)
    {
      using namespace std::literals;
      auto t = clk::now();
      if((t - last_ui_message) > 30ms)
      {
        last_ui_message = t;
        send_message(
            {.bytes = std::vector<unsigned char>(
                 in_tex.bytes, in_tex.bytes + in_tex.bytesize()),
             .w = in_tex.width,
             .h = in_tex.height});
      }
    }
  }

  // UI layout definition... we only have one widget here
  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(width, 64)
    halp_meta(height, 64)

    struct bus
    {
      static void process_message(ui& self, processor_to_ui msg)
      {
        self.tex.update(std::move(msg.bytes), msg.w, msg.h);
      }
    };

    halp::custom_actions_item<LightnessComputerTextureDisplay> tex;
    halp::control<&ins::size> size;
  };
};
}
