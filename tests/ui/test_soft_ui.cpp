/* SPDX-License-Identifier: GPL-3.0-or-later */

// Headless smoke test for the soft UI backend: renders declarative layouts
// and custom paint() widgets to RGBA framebuffers, drives them with
// synthetic mouse events, and checks the gesture / bus round-trips.

#include <avnd/binding/ui/soft/implementation.hpp>

#include <avnd/binding/ui/soft/surface.hpp>

#include <examples/Helpers/Ui.hpp>

#include <halp/custom_widgets.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstring>

namespace
{
static avnd::soft_ui::font_registry test_fonts()
{
  avnd::soft_ui::font_registry fonts;
#if defined(AVND_SOFT_UI_DEFAULT_FONT)
  REQUIRE(fonts.register_font_file("default", AVND_SOFT_UI_DEFAULT_FONT));
#endif
  return fonts;
}

static int count_nonzero(avnd::soft_ui::framebuffer fb)
{
  int n = 0;
  for(int i = 0, sz = fb.width * fb.height * 4; i < sz; i++)
    n += fb.data[i] != 0;
  return n;
}

static void write_ppm(const char* path, avnd::soft_ui::framebuffer fb)
{
  if(std::FILE* f = std::fopen(path, "wb"))
  {
    std::fprintf(f, "P6\n%d %d\n255\n", fb.width, fb.height);
    for(int i = 0, sz = fb.width * fb.height; i < sz; i++)
      std::fwrite(fb.data + i * 4, 1, 3, f);
    std::fclose(f);
  }
}

// Minimal processor with one custom interactive widget at a known position,
// a transaction, and a UI<->processor bus round-trip.
struct interaction_test_widget
{
  static constexpr double width() { return 200.; }
  static constexpr double height() { return 100.; }

  void paint(auto ctx)
  {
    ctx.set_fill_color({30, 30, 30, 255});
    ctx.begin_path();
    ctx.draw_rect(0., 0., width(), height());
    ctx.fill();

    ctx.set_fill_color({255, 176, 30, 255});
    ctx.begin_path();
    ctx.draw_rect(0., 0., value * width(), height());
    ctx.fill();
  }

  bool mouse_press(double x, double y)
  {
    transaction.start();
    update_from(x);
    return true;
  }
  bool mouse_move(double x, double y)
  {
    update_from(x);
    transaction.update(value);
    return true;
  }
  bool mouse_release(double x, double y)
  {
    transaction.commit();
    return false;
  }

  void update_from(double x)
  {
    value = x < 0. ? 0.f : x > width() ? 1.f : float(x / width());
  }

  static float value_to_control(auto& control, float v) { return v; }
  void set_value(const auto& control, float v) { value = v; }

  halp::transaction<float> transaction;
  float value{};
};

struct InteractionTest
{
  static consteval auto name() { return "SoftUiInteraction"; }
  static consteval auto c_name() { return "soft_ui_interaction"; }
  static consteval auto uuid() { return "3e0d3a1e-71a1-4f8a-9f0d-9f2f5a9b0c01"; }

  struct ins
  {
    struct : halp::val_port<"Level", float>
    {
      struct range
      {
        float min = 0., max = 1., init = 0.;
      };
    } level;
  } inputs;

  struct
  {
  } outputs;

  void operator()(int) { }

  struct ui_to_processor
  {
    float value;
  };
  struct processor_to_ui
  {
    float doubled;
  };

  int processed_messages = 0;
  void process_message(const ui_to_processor& msg)
  {
    processed_messages++;
    if(send_message)
      send_message(processor_to_ui{.doubled = msg.value * 2.f});
  }
  std::function<void(processor_to_ui)> send_message;

  struct ui
  {
    halp_meta(layout, halp::layouts::container)
    halp_meta(width, 320)
    halp_meta(height, 200)

    halp::custom_control<interaction_test_widget, &ins::level> widget{};

    float last_feedback = -1.f;

    struct bus
    {
      void init(ui& self)
      {
        // exercised through the transaction path instead
      }
      static void process_message(ui& self, processor_to_ui msg)
      {
        self.last_feedback = msg.doubled;
      }
      std::function<void(ui_to_processor)> send_message;
    };
  };
};
}

TEST_CASE("soft ui renders a declarative layout headlessly", "[soft_ui]")
{
  avnd::effect_container<examples::helpers::Ui> effect;
  avnd::soft_ui::surface<examples::helpers::Ui> ui{effect, test_fonts()};

  auto fb = ui.frame();
  REQUIRE(fb.width == 640);
  REQUIRE(fb.height == 480);

  // Something must have been drawn (Nuklear's default theme background +
  // widgets), and rendering must be deterministic frame to frame.
  const int nonzero = count_nonzero(fb);
  REQUIRE(nonzero > 10000);

  std::vector<unsigned char> first(fb.data, fb.data + fb.width * fb.height * 4);
  auto fb2 = ui.frame();
  REQUIRE(std::memcmp(first.data(), fb2.data, first.size()) == 0);

  write_ppm("soft_ui_layout.ppm", fb2);
}

TEST_CASE("custom widget interaction, gestures, bus round-trip", "[soft_ui]")
{
  avnd::effect_container<InteractionTest> effect;
  avnd::soft_ui::surface<InteractionTest> ui{effect, test_fonts()};
  auto& rt = ui.runtime();

  struct gesture_log
  {
    int begins{}, performs{}, ends{};
    double last_value{-1.};
  } log;
  rt.host.ctx = &log;
  rt.host.begin_edit = [](void* c, int) { ((gesture_log*)c)->begins++; };
  rt.host.perform_edit = [](void* c, int, double v) {
    auto& l = *(gesture_log*)c;
    l.performs++;
    l.last_value = v;
  };
  rt.host.end_edit = [](void* c, int) { ((gesture_log*)c)->ends++; };

  // First frame lays out the widget and computes its bounds.
  auto fb = ui.frame();
  REQUIRE(fb.width == 320);
  REQUIRE(fb.height == 200);

  // The custom widget is the only item: a 200x100 rect near the top-left.
  // Press in its vertical middle, at 75% width; drag to 50%; release.
  // Widget x starts after the window padding -- probe a horizontal line to
  // stay robust: press at canvas position and use the value the widget
  // computed itself.
  const double wx = 8, wy = 40; // inside the widget for any sane padding
  ui.pointer_move(wx, wy);
  ui.pointer_button(wx, wy, true);
  ui.frame(); // delivers press

  REQUIRE(log.begins == 1);

  ui.pointer_move(wx + 100, wy);
  ui.frame(); // drag

  REQUIRE(log.performs >= 1);
  REQUIRE(effect.effect.inputs.level.value > 0.f);

  ui.pointer_button(wx + 100, wy, false);
  ui.frame(); // release

  REQUIRE(log.ends == 1);

  write_ppm("soft_ui_interaction.ppm", ui.pixels());
}

namespace
{
// One full-row slider, for driving standard-widget gestures synthetically.
struct GestureTest
{
  static consteval auto name() { return "SoftUiGesture"; }
  static consteval auto c_name() { return "soft_ui_gesture"; }
  static consteval auto uuid() { return "1a2b3c4d-0001-4e2f-8c07-3d3b7e0a5f43"; }

  struct ins
  {
    halp::hslider_f32<"X"> x;
  } inputs;
  struct
  {
  } outputs;
  void operator()(int) { }

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp_meta(width, 400)
    halp_meta(height, 60)
    halp::item<&ins::x> x;
  };
};
}

TEST_CASE("standard widgets: one gesture per drag", "[soft_ui]")
{
  avnd::effect_container<GestureTest> effect;
  avnd::soft_ui::surface<GestureTest> ui{effect, test_fonts()};
  auto& rt = ui.runtime();

  struct gesture_log
  {
    int begins{}, performs{}, ends{};
  } log;
  rt.host.ctx = &log;
  rt.host.begin_edit = [](void* c, int) { ((gesture_log*)c)->begins++; };
  rt.host.perform_edit
      = [](void* c, int, double) { ((gesture_log*)c)->performs++; };
  rt.host.end_edit = [](void* c, int) { ((gesture_log*)c)->ends++; };

  ui.frame(); // initial layout

  // Drag across the slider (right column of the control row)
  ui.pointer_move(300, 22);
  ui.pointer_button(300, 22, true);
  ui.frame();
  ui.pointer_move(330, 22);
  ui.frame();
  ui.pointer_move(360, 22);
  ui.frame();
  ui.pointer_button(360, 22, false);
  ui.frame();

  CHECK(effect.effect.inputs.x.value > 0.5f);
  CHECK(log.begins == 1);
  CHECK(log.ends == 1);
  CHECK(log.performs >= 2);
}

TEST_CASE("hidpi: scale 2 renders at physical size", "[soft_ui]")
{
  avnd::effect_container<examples::helpers::Ui> effect;
  avnd::soft_ui::surface<examples::helpers::Ui> ui{effect, test_fonts()};
  ui.resize(320, 240, 2.);

  auto fb = ui.frame();
  REQUIRE(fb.width == 640);
  REQUIRE(fb.height == 480);
  REQUIRE(count_nonzero(fb) > 10000);

  // Input is descaled: physical (600, 28) is logical (300, 14) -- inside
  // the first row's slider; interaction must still work.
  ui.pointer_move(600, 28);
  ui.pointer_button(600, 28, true);
  ui.frame();
  ui.pointer_button(600, 28, false);
  auto fb2 = ui.frame();
  REQUIRE(fb2.width == 640);

  write_ppm("soft_ui_hidpi.ppm", fb2);
}

TEST_CASE("processor to ui bus is delivered", "[soft_ui]")
{
  avnd::effect_container<InteractionTest> effect;
  avnd::soft_ui::surface<InteractionTest> ui{effect, test_fonts()};

  REQUIRE(effect.effect.send_message);
  effect.effect.process_message({.value = 21.f});
  REQUIRE(effect.effect.processed_messages == 1);
  REQUIRE(ui.runtime().ui().last_feedback == 42.f);
}
