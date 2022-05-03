#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/processor.hpp>
#include <avnd/concepts/painter.hpp>
#include <avnd/wrappers/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <cmath>
#include <variant>
#include <cstdio>

namespace examples::helpers
{
struct custom_button
{
    static constexpr double width() { return 100.; }
    static constexpr double height() { return 100.; }

    void paint(avnd::painter auto ctx)
    {
      ctx.set_stroke_color({200, 200, 200, 255});
      ctx.set_stroke_width(2.);
      ctx.set_fill_color({100, 100, 100, 255});
      ctx.begin_path();
      ctx.draw_rounded_rect(0., 0., width(), height(), 5);
      ctx.fill();
      ctx.stroke();

      ctx.set_fill_color({0, 0, 0, 255});
      ctx.begin_path();
      ctx.draw_text(20., 20., fmt::format("{}", press_count));
      ctx.fill();
    }

    bool mouse_press(double x, double y)
    {
      on_pressed();
      return true;
    }

    void mouse_move(double x, double y)
    {
    }

    void mouse_release(double x, double y)
    {
    }

    int press_count{0};

    std::function<void()> on_pressed = [] { };
};

/**
 * This example shows how communication through a thread-safe message
 * bus between the UI and the engine can be achieved.
 *
 * The data flow in the example does a complete round-trip
 * from the UI to the engine and back to the UI again:
 *
 * - When the button is clicked in the UI, it calls custom_button::on_pressed
 * - In the ui::bus::init this generates a message from this, sent through
 *   ui::bus::send_message.
 * - Behind the scene, the bindings serialize the message and ask the host to kindly
 *   pass it to the engine thread.
 * - Engine thread receives the deserialized message in MessageBusUi::process_message
 * - Engine thread sends a feedback to the ui through MessageBusUi::send_message
 * - Bindings transfer it back to the ui thread, ui::bus::process_message(ui, the_message)
 *   gets called
 *
 * To implement multiple messages one can simply use std::variant as the argument type:
 *
 *   std::variant<message1, message2, etc...>
 *
 * Note that you don't have to implement serialization manually:
 * as long as the messages are aggregates, things happen magically :-)
 *
 */
struct MessageBusUi
{
  static consteval auto name() { return "MessageBusUi example"; }
  static consteval auto c_name() { return "avnd_mbus_ui"; }
  static consteval auto uuid() { return "4ed8e7fd-a1fa-40a7-bbbe-13ee50044248"; }

  struct { } inputs;
  struct { } outputs;

  void operator()(int N) { }

  // Here are some message types. Their type names do not matter, only that they are
  // aggregates. What matters is that they are used as arguments to process_message.

  // This one will be serialized / deserialized as it is not a trivial type
  struct ui_to_processor {
    int foo;
    std::string bar;
    std::variant<float, double> x;
    std::vector<float> y;
    struct {
      int x, y;
    } baz;
  };

  // This one will be memcpy'd as it is a trivial type
  struct processor_to_ui {
    float bar;
    struct {
      int x, y;
    } baz;
  };

  // 1. Receive a message on the processing thread from the UI
  void process_message(const ui_to_processor& msg)
  {
    fprintf(stderr, "Got message in processing thread !\n");
    send_message(processor_to_ui{.bar = 1.0, .baz{3, 4}});
  }

  // 2. Send a message from the processing thread to the ui
  std::function<void(processor_to_ui)> send_message;

  // Define our UI
  struct ui {
    halp_meta(layout, halp::layouts::container)
    halp_meta(width, 100)
    halp_meta(height, 100)

    struct {
      halp_meta(layout, halp::layouts::container)
      halp::custom_actions_item<custom_button> button{
          .x = 10
        , .y = 10
        // We'd like to define our callback here,
        // sadly C++ scoping rules do not allow it as soon as there is nesting
      };
    } foo;

    // Define the communication between UI and processor.
    struct bus {
      // 3. Set up connections
      void init(ui& ui)
      {
        ui.foo.button.on_pressed = [&] {
          fprintf(stderr, "Sending message from UI thread !\n");
          this->send_message(ui_to_processor{.foo = 123, .bar = "hiii", .x = 0.5f, .y = { 1, 3, 5 }});
        };
      }

      // 4. Receive a message on the UI thread from the processing thread
      static void process_message(ui& self, processor_to_ui msg)
      {
        fprintf(stderr, "Got message in ui thread ! %d %d\n", msg.baz.x, msg.baz.y);
        self.foo.button.press_count++;
      }

      // 5. Send a message from the ui to the processing thread
      std::function<void(ui_to_processor)> send_message;
    };
  };
};
}
