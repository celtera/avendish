#pragma once

#include <boost/functional/hash.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

namespace vo
{
// Graphical item which will display the texture
struct TextureDisplay
{
  using item_type = TextureDisplay;
  static constexpr double width() { return 200.; }
  static constexpr double height() { return 200.; }

  void paint(avnd::painter auto ctx)
  {
    if(m_w <= 0 || m_h <= 0)
      return;

    // Render the texture
    ctx.draw_bytes(0, 0, width(), height(), m_bytes.data(), m_w, m_h);

    // Render the circles
    std::uint32_t k = 342457370;
    for(auto [x, y] : m_positions)
    {
      // Used to give a different color to each circle according to their index
      k = (k * k * k) ^ (k - 1);
      std::uint32_t h = boost::hash_value(k);
      ctx.begin_path();
      ctx.set_fill_color(
          {.r = uint8_t((h & 0xFF0000) >> 16),
           .g = uint8_t((h & 0xFF00) >> 8),
           .b = uint8_t((h & 0xFF)),
           .a = 255});
      ctx.set_stroke_color({.r = 255, .g = 255, .b = 255, .a = 255});
      ctx.draw_circle(x * width(), y * height(), 5);
      ctx.stroke();
      ctx.fill();
    }
    ctx.update();
  }

  void update(
      std::vector<unsigned char>&& bytes, std::vector<halp::xy_type<float>>&& pos, int w,
      int h)
  {
    m_bytes = std::move(bytes);
    m_positions = std::move(pos);
    m_w = w;
    m_h = h;
  }

  std::vector<unsigned char> m_bytes = std::vector<unsigned char>(200 * 200 * 4);
  int m_w{0};
  int m_h{0};
  std::vector<halp::xy_type<float>> m_positions;
};

// Main processor object
struct LightnessSampler
{
public:
  halp_meta(name, "Lightness sampler");
  halp_meta(c_name, "lightness_sample");
  halp_meta(category, "Visuals/Computer Vision");
  halp_meta(author, "Jean-Michaël Celerier");
  halp_meta(description, "Sample the values of an image on multiple points.");
  halp_meta(uuid, "0e64750a-d014-44d9-9a4e-919d4305af1a");

  struct
  {
    halp::texture_input<"In"> image;
    halp::val_port<"Positions", std::vector<halp::xy_type<float>>> positions;
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
    std::vector<halp::xy_type<float>> positions;
  };
  std::function<void(processor_to_ui)> send_message;

  using clk = std::chrono::steady_clock;
  clk::time_point last_ui_message = clk::now();

  // Main processing
  void operator()()
  {
    auto& in_tex = inputs.image.texture;
    if(!in_tex.changed)
      return;

    if(!in_tex.bytes || in_tex.width <= 1 || in_tex.height <= 1)
      return;

    // Sample the texture
    auto& samples = outputs.samples.value;
    samples.clear();
    for(const auto [u, v] : inputs.positions.value)
    {
      const int x = std::clamp(int(u * in_tex.width), (int)0, (int)in_tex.width);
      const int y = std::clamp(int(v * in_tex.height), (int)0, (int)in_tex.height);

      const auto [r, g, b, a] = inputs.image.get(x, y);
      const auto lightness = linear_to_y(r / 255.f, g / 255.f, b / 255.f);
      const auto perceptual_lightness = y_to_lstar(lightness);
      samples.push_back(perceptual_lightness);
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
             .h = in_tex.height,
             .positions = std::move(inputs.positions.value)});
      }
    }
  }

  // UI layout definition... we only have one widget here
  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::container)
    halp_meta(width, 200)
    halp_meta(height, 200)

    struct bus
    {
      static void process_message(ui& self, processor_to_ui msg)
      {
        self.tex.update(std::move(msg.bytes), std::move(msg.positions), msg.w, msg.h);
      }
    };

    halp::custom_actions_item<TextureDisplay> tex{.x = 0, .y = 0};
  };
};
}
