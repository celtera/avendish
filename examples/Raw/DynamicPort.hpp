#pragma once
#include <cmath>

#include <functional>
#include <vector>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
struct SumPorts
{
  static constexpr auto name() { return "Sum"; }
  static constexpr auto c_name() { return "avnd_sumports"; }
  static constexpr auto author() { return "Jean-Michaël Celerier"; }
  static constexpr auto category() { return "Debug"; }
  static constexpr auto description()
  {
    return "Example of an object with dynamic number of inputs";
  }
  static constexpr auto uuid() { return "48b57b3e-227a-4a55-adce-86c011dcf491"; }

  struct inputs
  {
    struct
    {
      static constexpr auto name() { return "Control"; }
      enum widget
      {
        spinbox
      };
      struct range
      {
        int min = 0, max = 10, init = 0;
      };
      int value = 0;

      static std::function<void(SumPorts&, int)> on_controller_setup()
      {
        return [](SumPorts& object, int value) {
          object.inputs.in_i.request_port_resize(value);
          object.outputs.out_i.request_port_resize(value);
        };
      }
      static std::function<void(SumPorts&, int)> on_controller_interaction()
      {
        return [](SumPorts& object, int value) {
          object.inputs.in_i.request_port_resize(value);
          object.outputs.out_i.request_port_resize(value);
        };
      }
    } controller;

    struct
    {
      struct port
      {
        static constexpr auto name() { return "Input {}"; }
        struct range
        {
          double min = 0, max = 1, init = 0;
        };
        enum widget
        {
          knob
        };
        double value;
      };
      std::vector<port> ports{};
      std::function<void(int)> request_port_resize;
    } in_i;
  } inputs;

  struct
  {
    struct
    {
      static constexpr auto name() { return "Output"; }
      double value{};
    } out;
    struct
    {
      static constexpr auto name() { return "C"; }
      double value{};
    } c;

    struct
    {
      struct port
      {
        static constexpr auto name() { return "Output {}"; }
        double value;
      };
      std::vector<port> ports{};
      std::function<void(int)> request_port_resize;
    } out_i;
  } outputs;

  double filtered{};
  void operator()()
  {
    outputs.out.value = 0;
    int k = 0;
    const int N = inputs.in_i.ports.size();

    outputs.c.value = N;

    for(auto val : inputs.in_i.ports)
      outputs.out.value += std::pow(10, k++) * std::floor(10 * val.value);
    for(int i = 0; i < N; i++)
      outputs.out_i.ports[i].value = inputs.in_i.ports[i].value;
  }

  struct ui
  {
    static constexpr auto name() { return "Main"; }
    static constexpr auto layout()
    {
      enum
      {
        hbox
      };
      return hbox;
    }
    struct
    {
      static constexpr auto layout()
      {
        enum
        {
          control
        };
        return control;
      }
      decltype(&inputs::controller) model = &inputs::controller;
    } float_widget;

    struct
    {
      static constexpr auto layout()
      {
        enum
        {
          control
        };
        return control;
      }
      decltype(&inputs::in_i) model = &inputs::in_i;
    } multi_widget;
  };
};
}

// Adding a new concept.
// Process.
// 1. Specifying the concept:
// avnd/concepts/dynamic_port.hpp

// 2. Specify the introspection struct
// avnd/introspection/port.hpp

// 3. Specify the input or output (or both) introspection struct
// avnd/introspection/input.hpp

// 4. Create a filter
// avnd/binding/ossia/dynamic_ports.hpp
