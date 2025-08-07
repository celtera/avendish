#pragma once

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/value_types.hpp>

#include <cmath>
#include <functional>
#include <vector>

namespace uo
{

struct MultisliderWidget
{
  static constexpr double width() { return 600.; }
  static constexpr double height() { return 200.; }

  void paint(auto ctx)
  {
    // Draw background
    ctx.set_fill_color({20, 20, 20, 255});
    ctx.begin_path();
    ctx.draw_rect(0., 0., width(), height());
    ctx.fill();

    if (value.empty())
      return;

    // Calculate slider dimensions - simple rectangles side by side
    const double spacing = 2;
    const double sliderWidth = (width() - spacing * (value.size() - 1)) / value.size();
    
    // Draw sliders as simple rectangles
    int idx = 0;
    for (float val : value)
    {
      double x = idx * (sliderWidth + spacing);
      
      // Draw background rectangle
      ctx.set_fill_color({40, 40, 40, 255});
      ctx.begin_path();
      ctx.draw_rect(x, 0, sliderWidth, height());
      ctx.fill();
      
      // Draw filled portion from bottom
      double fillHeight = val * height();
      if (idx == selectedCursor)
      {
        ctx.set_fill_color({255, 200, 100, 255});
      }
      else
      {
        ctx.set_fill_color({100, 150, 255, 255});
      }
      ctx.begin_path();
      ctx.draw_rect(x, height() - fillHeight, sliderWidth, fillHeight);
      ctx.fill();
      
      // Draw runtime value if executing
      if (executing && idx < runtimeValues.size())
      {
        double runtimeHeight = runtimeValues[idx] * height();
        ctx.set_fill_color({255, 100, 100, 100});
        ctx.begin_path();
        ctx.draw_rect(x, height() - runtimeHeight, sliderWidth, runtimeHeight);
        ctx.fill();
      }
      
      idx++;
    }
  }

  bool mouse_press(double x, double y)
  {
    if (value.empty())
      return false;

    transaction.start();
    dragging = true;
    updateSliderAtPosition(x, y);
    return true;
  }

  bool mouse_move(double x, double y)
  {
    if (dragging)
    {
      updateSliderAtPosition(x, y);
      transaction.update(value);
      return true;
    }
    return false;
  }

  bool mouse_release(double x, double y)
  {
    if(dragging)
      transaction.commit();
    dragging = false;
    selectedCursor = -1;
    return false;
  }
  
  void updateSliderAtPosition(double x, double y)
  {
    // Calculate slider dimensions - same as in paint
    const double spacing = 2;
    const double sliderWidth = (width() - spacing * (value.size() - 1)) / value.size();
    
    // Find which slider is under the mouse
    for (int i = 0; i < value.size(); i++)
    {
      double sliderX = i * (sliderWidth + spacing);
      
      // Check if mouse is within this slider's horizontal bounds
      if (x >= sliderX && x <= sliderX + sliderWidth)
      {
        selectedCursor = i;
        // Update value based on y position (inverted - top is 1, bottom is 0)
        float newVal = 1.0 - std::clamp(y / height(), 0.0, 1.0);
        value[i] = newVal;
        if (on_value_changed)
          on_value_changed();
        break;
      }
    }
  }

  bool key(int key)
  {
    // Reset selected slider to default value with R key
    if (key == 'r' || key == 'R')
    {
      if (selectedCursor >= 0 && selectedCursor < value.size())
      {
        value[selectedCursor] = 0.5f;
        if (on_value_changed)
          on_value_changed();
        return true;
      }
    }
    return false;
  }

  void reset()
  {
    executing = false;
    runtimeValues.clear();
  }

  void set_value(const auto& control, std::vector<float> value) { this->value = value; }

  static auto value_to_control(auto& control, std::vector<float> value) { return value; }
  halp::transaction<std::vector<float>> transaction;

  std::vector<float> value;
  std::vector<float> runtimeValues;
  std::function<void()> on_value_changed;
  int selectedCursor = -1;
  int maxCursors = 10;
  bool executing = false;
  bool dragging = false;
};

struct Multislider
{
  halp_meta(name, "MultiSlider")
  halp_meta(c_name, "avnd_multislider")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "ossia score")
  halp_meta(description, "Multiple vertical sliders with dynamic count and bus communication")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/multislider.html")
  halp_meta(uuid, "8a7b4c5d-9e2f-4a3b-b1c6-d7e8f9a0b1c2")

  struct ins
  {
    struct : halp::spinbox_i32<"Count", halp::range{.min = 1, .max = 20, .init = 4}>
    {
      void update(Multislider& self) { self.updateCursorCount(); }
    } count;

    struct : halp::val_port<"Sliders", std::vector<float>>
    {
      enum widget
      {
        multi_slider
      };
      struct range
      {
        float min = 0., max = 1., init = 0.5;
      };
      void update(Multislider& self) { self.updateFromInput(); }
    } cursors;

  } inputs;

  struct outs
  {
    halp::val_port<"Cursors", std::vector<float>> cursors;
  } outputs;

  // Messages for UI communication
  struct cursor_update_message
  {
    halp_flag(relocatable);
    std::vector<float> cursors;
  };

  struct execution_value_to_ui
  {
    halp_flag(relocatable);
    std::vector<float> cursors;
  };

  // Process messages from UI
  void process_message(const cursor_update_message& msg)
  {
    inputs.cursors.value = msg.cursors;
    outputs.cursors.value = msg.cursors;
    update_ui();
  }

  // Send messages to UI
  std::function<void(std::variant<cursor_update_message, execution_value_to_ui>)>
      send_message;

  void updateCursorCount()
  {
    int newCount = inputs.count.value;
    int currentCount = inputs.cursors.value.size();
    
    if (newCount > currentCount)
    {
      // Add new sliders with default value 0.5
      for (int i = currentCount; i < newCount; i++)
      {
        inputs.cursors.value.push_back(0.5f);
      }
    }
    else if (newCount < currentCount)
    {
      // Remove sliders from the end
      inputs.cursors.value.resize(newCount);
    }
    
    outputs.cursors.value = inputs.cursors.value;
    update_ui();
  }

  void updateFromInput()
  {
    outputs.cursors.value = inputs.cursors.value;
    update_ui();
  }

  void update_ui()
  {
    if (send_message)
    {
      send_message(execution_value_to_ui{.cursors = inputs.cursors.value});
    }
  }

  using tick = halp::tick;
  void operator()(halp::tick t)
  {
    update_ui();
    for(float v : inputs.cursors.value)
      fprintf(stderr, "%f ", v);
    fprintf(stderr, "\n");
    outputs.cursors.value = inputs.cursors.value;
  }

  struct ui
  {
    using enum halp::colors;
    using enum halp::layouts;

    halp_meta(name, "Main")
    halp_meta(layout, vbox)
    halp_meta(background, dark)

    struct
    {
      halp_meta(layout, hbox)
      halp::control<&ins::count> count;
    } controls;

    halp::custom_control<MultisliderWidget, &ins::cursors> cursorArea;

    void start() 
    { 
      cursorArea.executing = true;
    }
    
    void stop()
    {
      cursorArea.reset();
    }
    
    void reset() 
    { 
      stop();
    }

    void on_control_update() { cursorArea.value.resize(controls.count.value); }
    struct bus
    {
      void init(ui& ui)
      {
        ui.cursorArea.maxCursors = ui.controls.count.value;
        
        // Initialize with proper default values if empty
        if (ui.cursorArea.value.empty())
        {
          for (int i = 0; i < ui.controls.count.value; i++)
          {
            ui.cursorArea.value.push_back(0.5f);
          }
        }

        ui.cursorArea.on_value_changed = [&ui, this] {
          this->send_message(
              Multislider::cursor_update_message{.cursors = ui.cursorArea.value});
        };
      }

      // Process messages from the processing thread
      static void do_process(ui& self, Multislider::cursor_update_message msg)
      {
        self.cursorArea.value = std::move(msg.cursors);
        // self.cursorArea.update();
      }

      static void do_process(ui& self, Multislider::execution_value_to_ui msg)
      {
        self.cursorArea.runtimeValues = std::move(msg.cursors);
        // self.cursorArea.update();
      }

      static void process_message(
          ui& self,
          std::variant<
              Multislider::cursor_update_message, Multislider::execution_value_to_ui>
              msg)
      {
        std::visit([&self](auto& m) { do_process(self, m); }, msg);
      }

      std::function<void(Multislider::cursor_update_message)> send_message;
    };
  };
};

}
