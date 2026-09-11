#pragma once
#include <avnd/binding/ossia/current_buffer_midi.hpp>
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

#include <vector>
#include <span>

namespace oscr
{
static inline auto& thread_local_midi_1to2_converter_instance()
{
  static thread_local libremidi::midi1_to_midi2 conv;
  return conv;
}

template <typename Exec_T, typename Obj_T>
struct process_after_run
{
  Exec_T& self;
  Obj_T& impl;
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
  void operator()(
      Field& ctrl, std::vector<ossia::audio_inlet*>& port,
      avnd::field_index<Idx>) const noexcept
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

  template <avnd::parameter_port Field, std::size_t Idx>
    requires ossia_port<Field>
  void write_value(
      Field& ctrl, auto& port, auto& val, int64_t ts,
      avnd::field_index<Idx>) const noexcept
  {
  }
  template <avnd::parameter_port Field, std::size_t Idx>
    requires(!ossia_port<Field>)
  void write_value(
      Field& ctrl, ossia::value_outlet& port, auto& val, int64_t ts,
      avnd::field_index<Idx> idx) const noexcept
  {
    if(auto v = to_ossia_value(ctrl, val); v.valid())
    {
      port->write_value(std::move(v), ts);

      if constexpr(avnd::control_port<Field>)
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

  template <avnd::parameter_port Field, std::size_t Idx>
    requires(!avnd::sample_accurate_parameter_port<Field> && !avnd::tensor_port<Field>
             && !ossia_port<Field>)
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    write_value(ctrl, port, ctrl.value, 0, avnd::field_index<Idx>{});
  }

  template <avnd::tensor_port Field, std::size_t Idx>
    requires(!ossia_port<Field>)
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    if(auto v = to_ossia_value(ctrl, ctrl.value); v.valid())
      port->write_value(std::move(v), 0);
  }

  template <avnd::dynamic_ports_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, std::vector<ossia::value_outlet*>& port,
      avnd::field_index<Idx>) const noexcept
  {
    int N = port.size();
    assert(N == ctrl.ports.size());
    if constexpr(!ossia_port<avnd::dynamic_port_type<Field>>)
      for(int i = 0; i < N; i++)
      {
        // FIXME double-check all the "0", most likely they should be the tick start timestamp instead
        write_value(
            ctrl.ports[i], *port[i], ctrl.ports[i].value, 0, avnd::field_index<Idx>{});
      }
  }

  template <avnd::linear_sample_accurate_parameter_port Field, std::size_t Idx>
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

  template <avnd::dynamic_sample_accurate_parameter_port Field, std::size_t Idx>
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
  template <avnd::span_sample_accurate_parameter_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
      = delete;

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }
  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, std::vector<ossia::audio_outlet*>& port,
      avnd::field_index<Idx>) const noexcept
  {
  }

  template <std::size_t Idx, typename Messages>
  void write_midi_output(const Messages& messages, ossia::midi_outlet& port) const noexcept
  {
    auto& output = [&]() -> auto& {
      if constexpr(use_local_midi_tick_batch<Obj_T>)
        return self.midi_tick_batch.template messages<Idx>();
      else
      {
        port.data.messages.clear();
        return port.data.messages;
      }
    }();

    if(!messages.empty())
    {
      auto& conv = thread_local_midi_1to2_converter_instance();
      cmidi2_midi_conversion_context_initialize(&conv.context);
      if constexpr(!use_local_midi_tick_batch<Obj_T>)
        output.reserve(messages.size());
      for(const auto& m : messages)
      {
        if constexpr(std::is_same_v<std::remove_cvref_t<decltype(m)>, libremidi::ump>)
        {
          auto packet = m;
          packet.timestamp = start + m.timestamp;
          output.push_back(packet);
        }
        else
        {
          const auto* bytes = std::data(m.bytes);
          const auto size = std::size(m.bytes);
          libremidi::ump packet{};
          // The stream converter consumes bank/RPN controllers as state, which
          // its per-invocation reset would then drop.
          if(size >= 2 && size <= 3 && bytes[0] >= 0x80 && bytes[0] < 0xf0
             && (size == 3 || (bytes[0] & 0xe0) == 0xc0)
             && cmidi2_midi1_channel_voice_to_midi2(bytes, size, packet.data))
          {
            packet.timestamp = start + m.timestamp;
            output.push_back(packet);
            continue;
          }
          conv.convert(
              bytes, size, start + m.timestamp,
              [&](const uint32_t* ump, int count, int64_t ts) {
            while(count > 0)
            {
              const int words = cmidi2_ump_get_num_bytes(*ump) / 4;
              if(words <= 0 || words > count)
                return stdx::error{std::errc::invalid_argument};
              libremidi::ump packet{};
              std::copy_n(ump, words, packet.data);
              packet.timestamp = ts;
              output.push_back(packet);
              ump += words;
              count -= words;
            }
            return stdx::error{};
          });
        }
      }
    }

    if constexpr(use_local_midi_tick_batch<Obj_T>)
    {
      // Unconditional: a consumer may have moved or cleared the port between slices.
      port.data.messages.assign(output.begin(), output.end());
    }
  }

  template <avnd::raw_container_midi_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::midi_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    write_midi_output<Idx>(
        std::span{ctrl.midi_messages, std::size_t(ctrl.size)}, port);
  }

  template <avnd::dynamic_container_midi_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::midi_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    write_midi_output<Idx>(ctrl.midi_messages, port);
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

  // Only done on gpu nodes now.
  template <avnd::geometry_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::geometry_outlet& port, avnd::field_index<Idx>) const noexcept
      = delete;
  /*
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
*/
};

}
