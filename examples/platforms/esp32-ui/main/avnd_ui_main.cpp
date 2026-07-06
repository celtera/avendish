/* SPDX-License-Identifier: GPL-3.0-or-later */

// Avendish soft UI on an ESP32 + SPI LCD (see README.md — illustrative,
// not yet run on hardware). The flow is the plan's §6.3 "windowless shell":
//
//   surface.pointer_button(touch...);   // push input
//   fb = surface.frame();               // widget pass + rasterize
//   rgba8 -> rgb565 -> esp_lcd flush    // your platform's blit

#include <avnd/binding/ui/soft/implementation.hpp>

#include <avnd/binding/ui/soft/surface.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr int LCD_W = 320, LCD_H = 240;

// The demo plug-in: any avendish processor with a `ui` works here.
struct Esp32Demo
{
  halp_meta(name, "ESP32 demo")
  halp_meta(c_name, "avnd_esp32_demo")
  halp_meta(uuid, "6f7c1188-6a3e-4f0d-9436-5f8c07e0a5f4")

  struct ins
  {
    halp::knob_f32<"Gain", halp::range{0., 1., .5}> gain;
    halp::toggle<"Mute"> mute;
  } inputs;
  struct
  {
  } outputs;
  void operator()(int) { }

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp_meta(width, LCD_W)
    halp_meta(height, LCD_H)
    halp::item<&ins::gain> gain;
    halp::item<&ins::mute> mute;
  };
};

// TTF embedded through EMBED_FILES in CMakeLists.txt
extern const unsigned char font_start[] asm("_binary_font_ttf_start");
extern const unsigned char font_end[] asm("_binary_font_ttf_end");

extern "C" void app_main()
{
  // ---- Panel setup: adapt to your board (this sketch assumes an SPI
  // ILI9341-style panel already configured elsewhere / via menuconfig) ----
  esp_lcd_panel_handle_t panel = nullptr;
  // ... esp_lcd_new_panel_io_spi + esp_lcd_new_panel_ili9341 + init ...

  // ---- UI setup ----
  static avnd::effect_container<Esp32Demo> effect;
  avnd::init_controls(effect);

  avnd::soft_ui::font_registry fonts;
  fonts.register_font(
      "default", std::vector<unsigned char>(font_start, font_end));

  static avnd::soft_ui::surface<Esp32Demo> ui{effect, std::move(fonts)};
  // Framebuffers live in PSRAM (see README.md for the memory budget)
  auto* rgb565 = (uint16_t*)heap_caps_malloc(
      size_t(LCD_W) * LCD_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  for(;;)
  {
    // ---- Touch: hook your controller here (esp_lcd_touch etc.) ----
    // if(touch_pressed) ui.pointer_button(x, y, true); ...

    const auto fb = ui.frame();

    // RGBA8 -> RGB565
    const auto* src = fb.data;
    for(int i = 0, n = fb.width * fb.height; i < n; i++, src += 4)
      rgb565[i] = uint16_t(
          ((src[0] & 0xF8) << 8) | ((src[1] & 0xFC) << 3) | (src[2] >> 3));

    if(panel)
      esp_lcd_panel_draw_bitmap(panel, 0, 0, fb.width, fb.height, rgb565);

    vTaskDelay(pdMS_TO_TICKS(33)); // ~30 fps
  }
}
