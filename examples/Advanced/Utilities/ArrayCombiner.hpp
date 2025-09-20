
#pragma once
#include <cmath>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
enum class ArrayCombinerMode
{
  Sum,
  Append,
  Product,
  Intersperse,
  Min,
  Max,
};
struct ArrayCombiner
{
  halp_meta(name, "Array Value Combiner")
  halp_meta(c_name, "avnd_array_combiner")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      description, "Combine the values of input arrays element-wise: sum, append, etc.")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/array-utilities.html#sum")
  halp_meta(uuid, "f5b8e20e-a016-4e19-a396-39d780179533")

  struct
  {
    struct : halp::spinbox_i32<"Input count", halp::range{0, 1024, 1}>
    {
      static std::function<void(ArrayCombiner&, int)> on_controller_interaction()
      {
        return [](ArrayCombiner& object, int value) {
          object.inputs.in_i.request_port_resize(value);
        };
      }
    } controller;

    halp::enum_t<ao::ArrayCombinerMode, "Mode"> mode;
    halp::dynamic_port<halp::val_port<"Input {}", std::vector<float>>> in_i;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::vector<float>> out;
  } outputs;

  void operator()()
  {
    const auto num_ports = inputs.in_i.ports.size();
    if(num_ports == 0)
      return;

    auto& out = outputs.out.value;
    if(num_ports == 1)
    {
      out = inputs.in_i.ports[0].value;
      return;
    }

    out.clear();
    switch(inputs.mode)
    {
      case ao::ArrayCombinerMode::Sum: {
        for(auto& v : inputs.in_i.ports)
        {
          if(v.value.size() > out.size())
          {
            out.resize(v.value.size());
          }
          for(std::size_t i = 0; i < v.value.size(); i++)
          {
            out[i] += v.value[i];
          }
        }

        break;
      }
      case ao::ArrayCombinerMode::Product: {
        for(auto& v : inputs.in_i.ports)
        {
          if(v.value.size() > out.size())
          {
            out.resize(v.value.size(), 1.f);
          }
          for(std::size_t i = 0; i < v.value.size(); i++)
          {
            out[i] *= v.value[i];
          }
        }

        break;
      }
      case ao::ArrayCombinerMode::Append: {
        std::size_t total_n = 0;
        for(auto& v : inputs.in_i.ports)
          total_n += v.value.size();
        out.reserve(total_n);

        for(auto& v : inputs.in_i.ports)
          out.insert(out.end(), v.value.begin(), v.value.end());

        break;
      }
      case ao::ArrayCombinerMode::Intersperse: {
        std::size_t max_n = 0;
        for(auto& v : inputs.in_i.ports)
          max_n = std::max(max_n, v.value.size());

        const auto ports = inputs.in_i.ports.size();
        out.resize(max_n * ports);

        for(std::size_t p = 0; p < ports; p++)
        {
          auto& in = inputs.in_i.ports[p].value;
          for(std::size_t i = 0; i < max_n; i++)
          {
            if(in.size() > i)
              out[i * ports + p] = in[i];
          }
        }
        break;
      }

      case ao::ArrayCombinerMode::Min: {
        // 1. Find the longest array
        int longest_v = 0;
        int longest_i = 0;
        for(int port_i = 0; port_i < num_ports; port_i++)
        {
          const int sz = inputs.in_i.ports[port_i].value.size();
          if(longest_v < sz)
          {
            longest_v = sz;
            longest_i = port_i;
          }
        }

        out = inputs.in_i.ports[longest_i].value;

        // 2. Min with the other arrays
        for(int port_i = 0; port_i < num_ports; port_i++)
        {
          if(port_i == longest_i)
            continue;

          auto& v = inputs.in_i.ports[port_i];
          for(std::size_t i = 0; i < v.value.size(); i++)
          {
            out[i] = std::min(out[i], v.value[i]);
          }
        }
        break;
      }

      case ao::ArrayCombinerMode::Max: {
        // 1. Find the longest array
        int longest_v = 0;
        int longest_i = 0;
        for(int port_i = 0; port_i < num_ports; port_i++)
        {
          const int sz = inputs.in_i.ports[port_i].value.size();
          if(longest_v < sz)
          {
            longest_v = sz;
            longest_i = port_i;
          }
        }

        out = inputs.in_i.ports[longest_i].value;

        // 2. Max with the other arrays
        for(int port_i = 0; port_i < num_ports; port_i++)
        {
          if(port_i == longest_i)
            continue;

          auto& v = inputs.in_i.ports[port_i];
          for(std::size_t i = 0; i < v.value.size(); i++)
          {
            out[i] = std::max(out[i], v.value[i]);
          }
        }
        break;
      }
    }
  }
};
}
