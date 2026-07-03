/* SPDX-License-Identifier: GPL-3.0-or-later */

// Avendish soft UI on a Soldered Inkplate 2: 212x104 black/white/red
// e-paper. A 3-color e-ink refreshes in ~15 s, so this is the render-once
// flavour of the soft runtime: build the UI, rasterize one frame headlessly
// (same path as the golden tests), quantize RGBA to the three inks, push to
// the panel, deep-sleep. The theme's orange value accents quantize to red.

#include "epd2.hpp"

#include <avnd/binding/ui/soft/implementation.hpp>

#include <avnd/binding/ui/soft/surface.hpp>
#include <halp/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

// TTF embedded through board_build.embed_files
extern const uint8_t font_start[] asm("_binary_src_font_ttf_start");
extern const uint8_t font_end[] asm("_binary_src_font_ttf_end");

static constexpr int W = 212, H = 104;

namespace demo
{
// Custom paint() widget: a big level bar, drawn with the same avnd::painter
// code that runs in the desktop plug-in editors.
struct level_bar
{
  static constexpr double width() { return 196.; }
  static constexpr double height() { return 30.; }

  void paint(auto ctx)
  {
    ctx.set_fill_color({45, 46, 52, 255});
    ctx.begin_path();
    ctx.draw_rounded_rect(0., 0., width(), height(), 4.);
    ctx.fill();

    ctx.set_fill_color({255, 176, 30, 255}); // -> red ink
    ctx.begin_path();
    ctx.draw_rounded_rect(2., 2., (width() - 4.) * value, height() - 4., 3.);
    ctx.fill();
  }

  float value{0.66f};
};

struct EinkDemo
{
  halp_meta(name, "Avendish e-ink")
  halp_meta(c_name, "avnd_eink_demo")
  halp_meta(uuid, "0be95a1e-90b7-4f6f-a5a4-6f30bb0a5f42")

  struct ins
  {
    halp::hslider_f32<"Gain", halp::range{0., 1., .66}> gain;
    halp::toggle<"Mute"> mute;
  } inputs;
  struct
  {
  } outputs;
  void operator()(int) { }

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp_meta(width, W)
    halp_meta(height, H)

    halp::item<&ins::gain> gain;
    halp::custom_item<demo::level_bar, &ins::gain> level{};
  };
};
}

// RGBA -> the three inks. The theme is dark surfaces / light text / orange
// accents: orange goes to red, bright goes to white, the rest to black.
static epd2::ink quantize(const unsigned char* px)
{
  const int r = px[0], g = px[1], b = px[2];
  if(r > 150 && r - b > 80 && r - g > 30)
    return epd2::red;
  const int luma = (r * 3 + g * 6 + b) / 10;
  return luma > 140 ? epd2::white : epd2::black;
}

void setup()
{
  Serial.begin(115200);
  Serial.println("avnd: rendering UI...");

  static avnd::effect_container<demo::EinkDemo> effect;
  avnd::init_controls(effect);

  avnd::soft_ui::font_registry fonts;
  fonts.register_font(
      "default", std::vector<unsigned char>(font_start, font_end));

  static avnd::soft_ui::surface<demo::EinkDemo> ui{effect, std::move(fonts)};
  ui.resize(W, H);

  const auto t0 = millis();
  const auto fb = ui.frame();
  Serial.printf(
      "avnd: rendered %dx%d in %lu ms, free heap %u, free psram %u\n", fb.width,
      fb.height, millis() - t0, ESP.getFreeHeap(), ESP.getFreePsram());

  Serial.println("avnd: pushing to e-paper (takes ~15 s)...");
  epd2::clear();
  for(int y = 0; y < fb.height; y++)
    for(int x = 0; x < fb.width; x++)
      epd2::set_pixel(x, y, quantize(fb.data + (size_t(y) * fb.width + x) * 4));
  const bool ok = epd2::show();
  Serial.printf("avnd: %s, sleeping.\n", ok ? "done" : "EPD TIMEOUT");
  esp_deep_sleep_start();
}

void loop() { }
