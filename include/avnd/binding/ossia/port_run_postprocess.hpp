#pragma once
#include <avnd/binding/ossia/geometry.hpp>
#include <avnd/binding/ossia/port_base.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <libremidi/detail/conversion.hpp>
// #include <halp/midi.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

namespace oscr
{
template <typename Exec_T, typename Obj_T>
struct process_after_run
{
  Exec_T& self;
  Obj_T& impl;
  int instance{};
  int& start = self.start_frame_for_this_tick;
  int& frames = self.frame_count_for_this_tick;

  template <typename Field, std::size_t Idx>
    requires ossia_port<Field>
  void operator()(Field& ctrl, auto& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    if_possible(ctrl.value.reset());
  }

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, std::vector<ossia::value_inlet*>& port,
      avnd::field_index<Idx>) const noexcept
  {
    for(auto& port_value : ctrl.ports)
    {
      if_possible(port_value.reset());
    }
  }

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <typename Field, std::size_t Idx>
  void
  operator()(Field& ctrl, ossia::midi_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::texture_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::geometry_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::parameter Field, std::size_t Idx>
    requires ossia_port<Field>
  void write_value(
      Field& ctrl, auto& port, auto& val, int64_t ts,
      avnd::field_index<Idx>) const noexcept
  {
  }
  template <avnd::parameter Field, std::size_t Idx>
    requires(!ossia_port<Field>)
  void write_value(
      Field& ctrl, ossia::value_outlet& port, auto& val, int64_t ts,
      avnd::field_index<Idx> idx) const noexcept
  {
    if(auto v = to_ossia_value(ctrl, val); v.valid())
    {
      port->write_value(std::move(v), ts);

      if constexpr(avnd::control<Field>)
      {
        // Get the index of the control in [0; N[
        using type = typename Exec_T::processor_type;
        using controls = avnd::control_output_introspection<type>;
        constexpr int control_index = controls::field_index_to_index(idx);

        // Mark the control as changed
        self.control.outputs_set.set(control_index);
      }
    }
  }

  template <avnd::parameter Field, std::size_t Idx>
    requires(!avnd::sample_accurate_parameter<Field> && !ossia_port<Field>)
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    write_value(ctrl, port, ctrl.value, 0, avnd::field_index<Idx>{});
  }

  template <avnd::dynamic_ports_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, std::vector<ossia::value_outlet*>& port,
      avnd::field_index<Idx>) const noexcept
  {
    int N = port.size();
    assert(N == ctrl.ports.size());
    for(int i = 0; i < N; i++)
    {
      // FIXME double-check all the "0", most likely they should be the tick start timestamp instead
      write_value(
          ctrl.ports[i], *port[i], ctrl.ports[i].value, 0, avnd::field_index<Idx>{});
    }
  }

  template <avnd::linear_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    auto& buffers = self.control_buffers.linear_inputs;
    // Idx is the index of the port in the complete input array.
    // We need to map it to the linear input index.
    using processor_type = typename Exec_T::processor_type;
    using lin_out = avnd::linear_timed_parameter_output_introspection<processor_type>;
    constexpr int storage_index = lin_out::field_index_to_index(idx);

    auto& buffer = get<storage_index>(buffers);

    for(int i = 0, N = self.buffer_size; i < N; i++)
    {
      if(buffer[i])
      {
        write_value(ctrl, port, *buffer[i], start + i, avnd::field_index<Idx>{});
        buffer[i] = {};
      }
    }
  }

  template <avnd::dynamic_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    for(auto& [timestamp, val] : ctrl.values)
    {
      write_value(ctrl, port, val, start + timestamp, avnd::field_index<Idx>{});
    }
    ctrl.values.clear();
  }

  // does not make sense as output, only as input
  template <avnd::span_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
      = delete;

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::raw_container_midi_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::midi_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    const int N = ctrl.midi_messages.size;
    port.data.messages.clear();
    port.data.messages.reserve(N);
    for(int i = 0; i < N; i++)
    {
      auto& m = ctrl.midi_messages[i];
      using msg_type = std::remove_reference_t<decltype(m)>;
      if constexpr(std::is_same_v<msg_type, libremidi::message>)
      {
        m.timestamp += start;
        port.data.messages.push_back(std::move(m));
      }
      else
      {
        libremidi::ump ms;
        cmidi2_midi1_channel_voice_to_midi2(m.bytes.data(), m.bytes.size(), ms.data);
        ms.timestamp = start + m.timestamp;

        port.data.messages.push_back(std::move(ms));
      }
    }
  }

  // TODO UMP ports
  // TODO note ports

  template <avnd::dynamic_container_midi_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::midi_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    port.data.messages.clear();
    port.data.messages.reserve(ctrl.midi_messages.size());
    for(auto& m : ctrl.midi_messages)
    {
      using msg_type = std::remove_reference_t<decltype(m)>;
      if constexpr(std::is_same_v<msg_type, libremidi::message>)
      {
        m.timestamp += start;
        port.data.messages.push_back(libremidi::ump_from_midi1(m));
      }
      else
      {
        libremidi::ump ms;
        cmidi2_midi1_channel_voice_to_midi2(m.bytes.data(), m.bytes.size(), ms.data);
        ms.timestamp = start + m.timestamp;

        port.data.messages.push_back(std::move(ms));
      }
    }
  }

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::texture_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::callback Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::geometry_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::geometry_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    using namespace avnd;
    bool mesh_dirty{};
    bool tform_dirty{};
    mesh_dirty = ctrl.dirty_mesh;

    if(ctrl.dirty_mesh)
    {
      port.data.geometry.meshes = std::make_shared<ossia::mesh_list>();
      auto& ossia_meshes = *port.data.geometry.meshes;
      if constexpr(static_geometry_type<Field> || dynamic_geometry_type<Field>)
      {
        ossia_meshes.meshes.resize(1);
        load_geometry(ctrl, ossia_meshes.meshes[0]);
      }
      else if constexpr(
          static_geometry_type<decltype(Field::mesh)>
          || dynamic_geometry_type<decltype(Field::mesh)>)
      {
        ossia_meshes.meshes.resize(1);
        load_geometry(ctrl.mesh, ossia_meshes.meshes[0]);
      }
      else
      {
        load_geometry(ctrl, ossia_meshes);
      }
    }
    ctrl.dirty_mesh = false;

    if constexpr(requires { ctrl.transform; })
    {
      if(ctrl.dirty_transform)
      {
        std::copy_n(
            ctrl.transform, std::ssize(ctrl.transform), port.data.transform.matrix);
        tform_dirty = true;
        ctrl.dirty_transform = false;
      }
    }

    port.data.flags = {};
    if(mesh_dirty)
      port.data.flags = port.data.flags | ossia::geometry_port::dirty_meshes;
    if(tform_dirty)
      port.data.flags = port.data.flags | ossia::geometry_port::dirty_transform;
  }
};

}
