#pragma once
#include <avnd/common/struct_reflection.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/input_introspection.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/graph_node.hpp>

namespace oscr
{
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



  template<avnd::parameter Field, std::size_t Idx>
  requires (!avnd::sample_accurate_parameter<Field>)
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::num<Idx>) const noexcept
  {
    port->write_value(ctrl.value, 0);
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
        port->write_value(*buffer[i], i);
        buffer[i] = {};
      }
    }
  }

  template<avnd::dynamic_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::num<Idx>) const noexcept
  {
    for(auto& [timestamp, val] : ctrl.values) {
      port->write_value(val, timestamp);
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
      port.data.messages.push_back({
                            .bytes = m.bytes
                          , .timestamp = m.timestamp
                          });
    }
  }

  template<avnd::dynamic_container_midi_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_outlet& port, avnd::num<Idx>) const noexcept
  {
    port.data.messages.clear();
    port.data.messages.reserve(ctrl.midi_messages.size());
    for(auto& m : ctrl.midi_messages)
    {
      port.data.messages.push_back({
                            .bytes = m.bytes
                          , .timestamp = m.timestamp
      });
    }
  }
};

}
