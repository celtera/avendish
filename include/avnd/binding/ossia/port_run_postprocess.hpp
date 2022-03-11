#pragma once
#include <avnd/common/struct_reflection.hpp>
#include <avnd/helpers/midi.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/input_introspection.hpp>
#include <avnd/wrappers/output_introspection.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/graph_node.hpp>

namespace oscr
{

template<typename T>
ossia::value to_ossia_value(T v)
{
  constexpr int sz = boost::pfr::tuple_size_v<T>;
  if constexpr(sz == 0)
  {
    return ossia::impulse{};
  }
  else if constexpr(sz == 2)
  {
    auto [x, y] = v;
    return ossia::vec2f{x, y};
  }
  else if constexpr(sz == 3)
  {
    auto [x, y, z] = v;
    return ossia::vec3f{x, y, z};
  }
  else if constexpr(sz == 4)
  {
    auto [x, y, z, w] = v;
    return ossia::vec4f{x, y, z, w};
  }
  else
  {
    static_assert(std::is_void_v<T>, "unsupported case");
    return ossia::value{};
  }
}

ossia::value to_ossia_value(std::integral auto v)
{ return v; }
ossia::value to_ossia_value(std::floating_point auto v)
{ return v; }
ossia::value to_ossia_value(avnd::string_ish auto v)
{ return std::string{v}; }
ossia::value to_ossia_value(avnd::enum_ish auto v)
{ return static_cast<int>(v); }
template<typename T>
requires std::is_enum_v<T>
ossia::value to_ossia_value(T v)
{ return static_cast<int>(v); }

inline ossia::value to_ossia_value(bool v)
{ return v; }

template <typename Exec_T>
struct process_after_run
{
  Exec_T& self;
  template<typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_inlet& port, avnd::num<Idx>) const noexcept
  {
  }

  template<typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::audio_inlet& port, avnd::num<Idx>) const noexcept
  {
  }

  template<typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_inlet& port, avnd::num<Idx>) const noexcept
  {
  }

  template<typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::texture_inlet& port, avnd::num<Idx>) const noexcept
  {
  }



  template<avnd::parameter Field, std::size_t Idx>
  requires (!avnd::control<Field>)
  void write_value(Field& ctrl, ossia::value_outlet& port, auto& val, int64_t ts, avnd::num<Idx>) const noexcept
  {
    port->write_value(ctrl.value, ts);
  }

  template<avnd::parameter Field, std::size_t Idx>
  requires (avnd::control<Field>)
  void write_value(Field& ctrl, ossia::value_outlet& port, auto& val, int64_t ts, avnd::num<Idx>) const noexcept
  {
    port->write_value(ctrl.value, ts);

    // Get the index of the control in [0; N[
    using type = typename Exec_T::processor_type;
    using controls = avnd::control_output_introspection<type>;
    constexpr int control_index = avnd::index_of_element(Idx, typename controls::indices_n{});

    // Mark the control as changed
    self.control.outputs_set.set(control_index);
  }

  template<avnd::parameter Field, std::size_t Idx>
  requires (!avnd::sample_accurate_parameter<Field>)
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::num<Idx>) const noexcept
  {
    write_value(ctrl, port, ctrl.value, 0, avnd::num<Idx>{});
  }

  template<avnd::linear_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::num<Idx>) const noexcept
  {
    auto& buffers = self.control_buffers.linear_inputs;
    // Idx is the index of the port in the complete input array.
    // We need to map it to the linear input index.
    using processor_type = typename Exec_T::processor_type;
    using lin_out = avnd::linear_timed_parameter_output_introspection<processor_type>;
    using indices = typename lin_out::indices_n;
    constexpr int storage_index = avnd::index_of_element(Idx, indices{});

    auto& buffer = std::get<storage_index>(buffers);

    for(int i = 0, N = self.buffer_size; i < N; i++)
    {
      if(buffer[i])
      {
        write_value(ctrl, port, *buffer[i], i, avnd::num<Idx>{});
        buffer[i] = {};
      }
    }
  }

  template<avnd::dynamic_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::num<Idx>) const noexcept
  {
    for(auto& [timestamp, val] : ctrl.values) {
      write_value(ctrl, port, val, timestamp, avnd::num<Idx>{});
    }
    ctrl.values.clear();
  }

  // does not make sense as output, only as input
  template<avnd::span_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::num<Idx>) const noexcept = delete;

  template<typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::audio_outlet& port, avnd::num<Idx>) const noexcept
  {
  }

  template<avnd::raw_container_midi_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_outlet& port, avnd::num<Idx>) const noexcept
  {
    const int N = ctrl.midi_messages.size;
    port.data.messages.clear();
    port.data.messages.reserve(N);
    for(int i = 0; i < N; i++)
    {
      auto& m = ctrl.midi_messages[i];
      libremidi::message ms;
      ms.bytes.assign(m.bytes.begin(), m.bytes.end());
      ms.timestamp = m.timestamp;
      port.data.messages.push_back(std::move(ms));
    }
  }

  template<avnd::dynamic_container_midi_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_outlet& port, avnd::num<Idx>) const noexcept
  {
    port.data.messages.clear();
    port.data.messages.reserve(ctrl.midi_messages.size());
    for(auto& m : ctrl.midi_messages)
    {
      libremidi::message ms;
      ms.bytes.assign(m.bytes.begin(), m.bytes.end());
      ms.timestamp = m.timestamp;
      port.data.messages.push_back(std::move(ms));
    }
  }

  template<typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::texture_outlet& port, avnd::num<Idx>) const noexcept
  {
  }

  template<avnd::callback Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::num<Idx>) const noexcept
  {
  }
};

}
