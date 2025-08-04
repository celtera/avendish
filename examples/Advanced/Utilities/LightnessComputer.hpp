#pragma once

#include <boost/functional/hash.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <chrono>

#include <halp/controls.enums.hpp>

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
  halp_meta(category, "Visuals/Analysis");
  halp_meta(author, "Jean-Michaël Celerier");
  halp_meta(description, "Convert an image to a list of lightness values.");
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/pixel-utilities.html#lightness-computer")
  halp_meta(uuid, "60a11c39-dc14-449d-bbe4-1c51e44ea99a");

  struct ins
  {
    halp::fixed_texture_input<"In"> image{.request_width = 16, .request_height = 16};
    halp::xy_spinboxes_i32<"Size", halp::range{1, 512, 10}> size;
    struct mode
    {
      halp__enum_combobox(
          "Mode", Lightness, Lightness, Value, Saturation, Hue, HSV, HSL, Red, Green,
          Blue, Alpha, RGB, RGBW)
    } mode;
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

  static std::array<double, 3> hsv(uint8_t r, uint8_t g, uint8_t b)
  {
    const double var_R = r / 255.f;
    const double var_G = g / 255.f;
    const double var_B = b / 255.f;

    const auto var_Min = std::min(std::min(var_R, var_G), var_B); // Min. value of RGB
    const auto var_Max = std::max(std::max(var_R, var_G), var_B); // Max. value of RGB
    const auto del_Max = var_Max - var_Min;                       // Delta RGB value

    if(del_Max == 0.) // This is a gray, no chroma...
    {
      return {0., 0., (double)var_Max};
    }
    else // Chromatic data...
    {
      double H{};
      auto S = del_Max / var_Max;
      auto V = var_Max;

      auto del_R = (((var_Max - var_R) / 6.) + (del_Max / 2.)) / del_Max;
      auto del_G = (((var_Max - var_G) / 6.) + (del_Max / 2.)) / del_Max;
      auto del_B = (((var_Max - var_B) / 6.) + (del_Max / 2.)) / del_Max;

      if(var_R == var_Max)
        H = del_B - del_G;
      else if(var_G == var_Max)
        H = (1. / 3.) + del_R - del_B;
      else if(var_B == var_Max)
        H = (2. / 3.) + del_G - del_R;

      if(H < 0.)
        H += 1.;
      if(H > 1.)
        H -= 1.;
      return {H, S, V};
    }
  }

  // https://gist.github.com/ciembor/1494530
  static std::array<double, 3> hsl(float r, float g, float b)
  {
    struct hsl
    {
      double h, s, l;
    } result;

    r /= 255;
    g /= 255;
    b /= 255;

    float max = std::max(std::max(r, g), b);
    float min = std::min(std::min(r, g), b);

    result.h = result.s = result.l = (max + min) / 2;

    if(max == min)
    {
      result.h = result.s = 0; // achromatic
    }
    else
    {
      float d = max - min;
      result.s = (result.l > 0.5) ? d / (2 - max - min) : d / (max + min);

      if(max == r)
      {
        result.h = (g - b) / d + (g < b ? 6 : 0);
      }
      else if(max == g)
      {
        result.h = (b - r) / d + 2;
      }
      else if(max == b)
      {
        result.h = (r - g) / d + 4;
      }

      result.h /= 6;
    }

    return {result.h, result.s, result.l};
  }

  //http://blog.saikoled.com/post/44677718712/how-to-convert-from-hsi-to-rgb-white
  static void hsi2rgbw(double h, double s, double i, int* rgbw)
  {
    using namespace std;
    int r, g, b, w;
    double cos_h, cos_1047_h;
    //h = fmod(h,360); // cycle h around to 0-360 degrees
    h = h * 2. * 3.14159; // * h = 3.14159 * h / 180.; // Convert to radians.
    // s /= 100.;
    // i /= 100.;                           //from percentage to ratio
    s = s > 0. ? (s < 1. ? s : 1.) : 0.; // clamp s and i to interval [0,1]
    i = i > 0. ? (i < 1. ? i : 1.) : 0.;
    i = i * sqrt(i); //shape intensity to have finer granularity near 0

    if(h < 2.09439)
    {
      cos_h = cos(h);
      cos_1047_h = cos(1.047196667 - h);
      r = s * 4095. * i / 3. * (1. + cos_h / cos_1047_h);
      g = s * 4095. * i / 3. * (1. + (1. - cos_h / cos_1047_h));
      b = 0.;
      w = 4095. * (1. - s) * i;
    }
    else if(h < 4.188787)
    {
      h = h - 2.09439;
      cos_h = cos(h);
      cos_1047_h = cos(1.047196667 - h);
      g = s * 4095. * i / 3. * (1. + cos_h / cos_1047_h);
      b = s * 4095. * i / 3. * (1. + (1. - cos_h / cos_1047_h));
      r = 0.;
      w = 4095. * (1. - s) * i;
    }
    else
    {
      h = h - 4.188787;
      cos_h = cos(h);
      cos_1047_h = cos(1.047196667 - h);
      b = s * 4095. * i / 3. * (1. + cos_h / cos_1047_h);
      r = s * 4095. * i / 3. * (1. + (1. - cos_h / cos_1047_h));
      g = 0.;
      w = 4095. * (1. - s) * i;
    }

    rgbw[0] = r;
    rgbw[1] = g;
    rgbw[2] = b;
    rgbw[3] = w;
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

  void prepare()
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

    int i = 0;
    auto apply = [this, &in_tex](auto func) mutable {
#pragma omp simd
      for(int y = 0; y < in_tex.height; y++)
      {
        auto row = inputs.image.row(y);
        for(int x = 0; x < in_tex.width; x++)
        {
          const auto [r, g, b, a] = inputs.image.get(x, row);
          func(r, g, b, a);
        }
      }
    };

    switch(inputs.mode.value)
    {
      case ins::mode::Lightness: {
        samples.resize(in_tex.height * in_tex.width);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
          const auto lightness = linear_to_y(r / 255.f, g / 255.f, b / 255.f);
          const auto perceptual_lightness = y_to_lstar(lightness);
          samples[i] = perceptual_lightness;
          i++;
        });
        break;
      }

      case ins::mode::Value: {
        samples.resize(in_tex.height * in_tex.width);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
          auto [h, s, v] = hsv(r, g, b);
          samples[i++] = v;
        });
        break;
      }
      case ins::mode::Hue: {
        samples.resize(in_tex.height * in_tex.width);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
          auto [h, s, v] = hsv(r, g, b);
          samples[i++] = h;
        });
        break;
      }
      case ins::mode::Saturation: {
        samples.resize(in_tex.height * in_tex.width);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
          auto [h, s, v] = hsv(r, g, b);
          samples[i++] = s;
        });
        break;
      }
      case ins::mode::HSL: {
        samples.resize(in_tex.height * in_tex.width * 3);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
          auto [h, s, l] = hsl(r, g, b);
          samples[i++] = h;
          samples[i++] = s;
          samples[i++] = l;
        });
        break;
      }

      case ins::mode::HSV: {
        samples.resize(in_tex.height * in_tex.width * 3);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
          auto [h, s, v] = hsv(r, g, b);
          samples[i++] = h;
          samples[i++] = s;
          samples[i++] = v;
        });
        break;
      }

      case ins::mode::Red: {
        samples.resize(in_tex.height * in_tex.width);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) { samples[i++] = r; });
        break;
      }
      case ins::mode::Green: {
        samples.resize(in_tex.height * in_tex.width);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) { samples[i++] = g; });
        break;
      }
      case ins::mode::Blue: {
        samples.resize(in_tex.height * in_tex.width);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) { samples[i++] = b; });
        break;
      }
      case ins::mode::Alpha: {
        samples.resize(in_tex.height * in_tex.width);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) { samples[i++] = a; });
        break;
      }
      case ins::mode::RGB: {
        samples.resize(in_tex.height * in_tex.width * 3);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
          samples[i++] = r;
          samples[i++] = g;
          samples[i++] = b;
        });
        break;
      }
      case ins::mode::RGBW: {
        samples.resize(in_tex.height * in_tex.width * 4);
        apply([&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
          const auto [h, s, l] = hsl(r, g, b);

          int rgbw[4];
          hsi2rgbw(h, s, l, rgbw);
          samples[i++] = rgbw[0];
          samples[i++] = rgbw[1];
          samples[i++] = rgbw[2];
          samples[i++] = rgbw[3];
        });
        break;
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
    halp_meta(height, 80)

    struct bus
    {
      static void process_message(ui& self, processor_to_ui msg)
      {
        self.tex.update(std::move(msg.bytes), msg.w, msg.h);
      }
    };

    halp::custom_actions_item<LightnessComputerTextureDisplay> tex;
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp::control<&ins::size> size;
      halp::control<&ins::mode> mode;
    } controls;
  };
};
}
