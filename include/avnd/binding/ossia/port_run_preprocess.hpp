#pragma once
#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/geometry.hpp>
#include <avnd/binding/ossia/port_base.hpp>
#include <avnd/binding/ossia/window_functions.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/fft.hpp>
#include <avnd/concepts/soundfile.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <fmt/printf.h>
#include <ossia/audio/fft.hpp>
#include <ossia/dataflow/dataflow.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/parse_relax.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/value/format_value.hpp>

namespace oscr
{

// If we have a polyphonic object
// And we get an array input that corresponds to the kind of data expected
// Then we apply the value element-wise to each monophonic instance
//
// To do this we must make sure that we always create as many "input" objects as needed
template <typename Value>
  requires std::is_arithmetic_v<Value>
struct apply_value_polyphonic_control
{
  Value& res;
  int instance;
  bool operator()() const { return false; }
  bool operator()(ossia::impulse) const
  {
    if_possible(res = {});
    return true;
  }

  bool operator()(float v) const
  {
    res = v;
    return true;
  }

  bool operator()(int v) const
  {
    res = v;
    return true;
  }

  bool operator()(bool v) const
  {
    res = v ? 1. : 0.;
    return true;
  }

  bool operator()(const std::string& v) const
  {
    if(auto f = ossia::parse_relax<double>(v))
    {
      res = *f;
      return true;
    }
    return false;
  }

  template <std::size_t N>
  bool operator()(const std::array<float, N>& v) const
  {
    if(instance >= 0 && instance < N)
    {
      res = v[instance];
      return true;
    }
    return false;
  }
  bool operator()(const std::vector<ossia::value>& v) const
  {
    if(instance >= 0 && instance < v.size())
    {
      res = ossia::convert<float>(v[instance]);
      return true;
    }
    return false;
  }
  bool operator()(const ossia::value_map_type& v) const { return false; }
};

struct node_from_destination
{
  ossia::net::node_base* operator()(ossia::net::parameter_base* p)
  {
    if(p)
      return &p->get_node();
  }
  // ossia::traversal::path, ossia::net::node_base*
};

template <typename Exec_T, typename Obj_T>
struct set_ossia_node_in_port
{
  Exec_T& self;
  Obj_T& impl;

  inline void operator()(auto& ctrl, auto& port, auto) const noexcept
  {
    if constexpr(requires { ctrl.ossia_node = nullptr; })
    {
      ctrl.ossia_node = impl.ossia_state.get_first_destination(port.address);
    }
  }
};

template <typename Exec_T, typename Obj_T>
struct process_before_run
{
  Exec_T& self;
  Obj_T& impl;
  int instance{};
  int start{};
  int frames{};

  template <typename Field, typename Value, std::size_t Idx>
  OSSIA_INLINE void update_value(
      Field& field, const ossia::value& src, Value& dst,
      avnd::field_index<Idx> idx) const
  {
    // TODO
    // if constexpr(oscr::real_good_mono_processor<Obj>)
    if(self.from_ossia_value(field, src, dst, idx))
    {
      if_possible(field.update(impl));
    }
  }

  // Some heads up for the most common cases, + handle mono deconstruction
  template <typename Field, typename Value, std::size_t Idx>
    requires std::is_arithmetic_v<Value>
  OSSIA_INLINE void update_value(
      Field& field, const ossia::value& src, Value& dst,
      avnd::field_index<Idx> idx) const
  {
    // FIXME time controls & smooth
    using eff_t = avnd::effect_container<Obj_T>;
    static constexpr bool multi_instance = requires { sizeof(eff_t::multi_instance); };
    if constexpr(
        multi_instance && !avnd::smooth_parameter<Field> && !avnd::time_control<Field>)
    {
      if(src.apply(apply_value_polyphonic_control<Value>{dst, instance}))
      {
        if_possible(field.update(impl));
      }
    }
    else
    {
      if(self.from_ossia_value(field, src, dst, idx))
      {
        if_possible(field.update(impl));
      }
    }
  }
  template <avnd::parameter Field, std::size_t Idx>
    requires ossia_port<Field>
  void init_value(Field& ctrl, auto& port, avnd::field_index<Idx> idx) const noexcept
  {
  }
  template <avnd::parameter Field, std::size_t Idx>
    requires ossia_port<Field>
  void operator()(Field& ctrl, auto& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::parameter Field, std::size_t Idx>
    requires(!ossia_port<Field>)
  void init_value(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    // FIXME ensure this is not called for a dynamic_ports
    if(!port.data.get_data().empty())
    {
      auto& last = port.data.get_data().back().value;
      update_value(ctrl, last, ctrl.value, idx);

      if constexpr(avnd::control<Field>)
      {
        // Get the index of the control in [0; N[
        using type = typename Exec_T::processor_type;
        using controls = avnd::control_input_introspection<type>;
        constexpr int control_index = controls::field_index_to_index(idx);

        // Mark the control as changed
        self.control.inputs_set.set(control_index);
      }
    }
  }

  template <avnd::dynamic_ports_port Field, std::size_t Idx>
  void init_value(
      Field& ctrl, std::vector<ossia::value_inlet*>& ports,
      avnd::field_index<Idx> idx) const noexcept
  {
    bool written = ports.size() != ctrl.ports.size();
    ctrl.ports.resize(ports.size());
    int p = 0;
    for(auto pport : ports)
    {
      auto& port = *pport;
      if(!port.data.get_data().empty())
      {
        auto& last = port.data.get_data().back().value;

        // FIXME check optional ports case
        written |= self.from_ossia_value(ctrl.ports[p], last, ctrl.ports[p].value, idx);

        // FIXME
        // if constexpr(avnd::control<Field>)
        // {
        //   // Get the index of the control in [0; N[
        //   using type = typename Exec_T::processor_type;
        //   using controls = avnd::control_input_introspection<type>;
        //   constexpr int control_index = controls::field_index_to_index(idx);

        //   // Mark the control as changed
        //   self.control.inputs_set.set(control_index);
        // }
      }
      ++p;
    }

    if(written)
    {
      if_possible(ctrl.update(impl));
    }
  }

  template <avnd::parameter Field, std::size_t Idx>
    requires(
        !avnd::sample_accurate_parameter<Field> && !avnd::span_parameter<Field>
        && !ossia_port<Field>)
  void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    init_value(ctrl, port, avnd::field_index<Idx>{});
  }

  template <avnd::dynamic_ports_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, std::vector<ossia::value_inlet*>& port,
      avnd::field_index<Idx> idx) const noexcept
  {
    init_value(ctrl, port, avnd::field_index<Idx>{});
  }

  template <avnd::linear_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    // FIXME we need to know about the buffer size !
    init_value(ctrl, port, avnd::field_index<Idx>{});

    for(auto& [val, ts] : port->get_data())
    {
      if(ts >= start && ts < start + frames)
      {
        auto& v = ctrl.values[ts - start].emplace();
        update_value(self, impl, ctrl, val, v, avnd::field_index<Idx>{});
      }
    }
  }

  template <avnd::span_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    init_value(ctrl, port, avnd::field_index<Idx>{});
    for(auto& [val, ts] : port->get_data())
    {
      // FIXME
    }
  }

  template <avnd::dynamic_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    init_value(ctrl, port, avnd::field_index<Idx>{});
    for(auto& [val, ts] : port->get_data())
    {
      if(ts >= start && ts < start + frames)
      {
        update_value(ctrl, val, ctrl.values[ts - start], avnd::field_index<Idx>{});
      }
    }
  }

  template <avnd::spectrum_split_channel_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    if(port.data.channels() >= 1)
    {
      using sc_in = avnd::spectrum_split_channel_input_introspection<
          typename Exec_T::processor_type>;
      static constexpr auto fft_idx = sc_in::field_index_to_index(idx);
      ossia::fft& fft = get<fft_idx>(self.spectrums.split_channel.ffts);

      auto& samples = port.data.channel(0);
      const int N = samples.size();
      if(N > 0)
      {
        apply_window{}(ctrl, samples.data(), fft.input(), N);

        auto fftOut = reinterpret_cast<ossia::fft_real*>(fft.execute());
        deinterleave(fftOut, N);

        ctrl.spectrum.amplitude = fftOut;
        ctrl.spectrum.phase = fftOut + N / 2;
      }
    }
  }

  template <avnd::spectrum_complex_channel_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    if(port.data.channels() >= 1)
    {
      using sc_in = avnd::spectrum_complex_channel_input_introspection<
          typename Exec_T::processor_type>;
      static constexpr auto fft_idx = sc_in::field_index_to_index(idx);
      ossia::fft& fft = get<fft_idx>(self.spectrums.complex_channel.ffts);

      auto& samples = port.data.channel(0);
      const int N = samples.size();
      if(N > 0)
      {
        apply_window{}(ctrl, samples.data(), fft.input(), N);

        ctrl.spectrum.bin = reinterpret_cast<decltype(ctrl.spectrum.bin)>(fft.execute());
      }
    }
  }

  template <avnd::spectrum_split_bus_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    if(port.data.channels() >= 1)
    {
      using sc_in = avnd::spectrum_split_bus_input_introspection<
          typename Exec_T::processor_type>;
      static constexpr auto fft_idx = sc_in::field_index_to_index(idx);
      auto& ffts = get<fft_idx>(self.spectrums.split_bus.ffts);

      for(std::size_t c = 0; c < port.data.channels(); c++)
      {
        auto& samples = port.data.channel(c);
        auto& fft = ffts.ffts[c];
        const int N = samples.size();
        if(N > 0)
        {
          apply_window{}(ctrl, samples.data(), fft.input(), N);

          auto fftOut = reinterpret_cast<ossia::fft_real*>(fft.execute());
          deinterleave(fftOut, N);

          ctrl.spectrum.amplitude[c] = fftOut;
          ctrl.spectrum.phase[c] = fftOut + N / 2;
        }
      }
    }
  }

  template <avnd::spectrum_complex_bus_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    if(port.data.channels() >= 1)
    {
      using sc_in = avnd::spectrum_split_bus_input_introspection<
          typename Exec_T::processor_type>;
      static constexpr auto fft_idx = sc_in::field_index_to_index(idx);
      auto& ffts = get<fft_idx>(self.spectrums.complex_bus.ffts);

      for(std::size_t c = 0; c < port.data.channels(); c++)
      {
        auto& samples = port.data.channel(c);
        auto& fft = ffts.ffts[c];
        const int N = samples.size();
        if(N > 0)
        {
          apply_window{}(ctrl, samples.data(), fft.input(), N);

          ctrl.spectrum.bin[c]
              = reinterpret_cast<decltype(ctrl.spectrum.bin[c])>(fft.execute());
        }
      }
    }
  }

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::raw_container_midi_port Field, std::size_t Idx>
  void
  operator()(Field& ctrl, ossia::midi_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    // FIXME
  }

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::texture_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::span_parameter Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    auto& dat = port.data.get_data();
    if(dat.empty())
      return;

    auto& back = dat.back();
    auto src_vec = back.value.template target<std::vector<ossia::value>>();
    if(!src_vec)
      return;

    using span_in = avnd::span_parameter_input_introspection<Obj_T>;
    using T = typename decltype(Field::value)::value_type;
    static constexpr auto field_idx
        = span_in::field_index_to_index(avnd::field_index<Idx>{});
    auto& dest_vec = get<field_idx>(self.control_buffers.span_inputs);
    dest_vec.resize(src_vec->size());

    for(std::size_t i = 0; i < src_vec->size(); i++)
    {
      self.from_ossia_value(ctrl, (*src_vec)[i], dest_vec[i], idx);
    }

    ctrl.value = dest_vec;

    if_possible(ctrl.update(impl));
  }

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::geometry_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    if(port.data.flags & port.data.dirty_meshes)
      ctrl.dirty_mesh = true;
    if(port.data.flags & port.data.dirty_transform)
      ctrl.dirty_transform = true;
    geometry_from_ossia(port.data, ctrl);
  }

  template <avnd::soundfile_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    auto& dat = port.data.get_data();
    if(dat.empty())
      return;

    auto& back = dat.back();
    auto str = back.value.template target<std::string>();
    if(!str)
      return;

    self.soundfiles.load_request(*str, Idx);
  }

  template <avnd::midifile_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    auto& dat = port.data.get_data();
    if(dat.empty())
      return;

    auto& back = dat.back();
    auto str = back.value.template target<std::string>();
    if(!str)
      return;

    self.midifile_load_request(*str, Idx);
  }

  template <avnd::raw_file_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    auto& dat = port.data.get_data();
    if(dat.empty())
      return;

    auto& back = dat.back();
    auto str = back.value.template target<std::string>();
    if(!str)
      return;

    self.raw_file_load_request(*str, Idx);
  }

  template <avnd::dynamic_container_midi_port Field, std::size_t Idx>
  void
  operator()(Field& ctrl, ossia::midi_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    using midi_msg_type =
        typename std::remove_cvref_t<decltype(Field::midi_messages)>::value_type;

    if constexpr(std::is_same_v<midi_msg_type, libremidi::message>)
    {
      ctrl.midi_messages.reserve(port.data.messages.size());
      for(const libremidi::message& msg_in : port.data.messages)
      {
        if(msg_in.timestamp >= start && msg_in.timestamp < start + frames)
        {
          ctrl.midi_messages.push_back(msg_in);
          ctrl.midi_messages.back().timestamp -= start;
        }
      }
    }
    else
    {
      ctrl.midi_messages.reserve(port.data.messages.size());
      for(const libremidi::message& msg_in : port.data.messages)
      {
        if(msg_in.timestamp >= start && msg_in.timestamp < start + frames)
        {
          ctrl.midi_messages.push_back(
              {.bytes{msg_in.begin(), msg_in.end()}, .timestamp{(int)msg_in.timestamp}});
          ctrl.midi_messages.back().timestamp -= start;
        }
      }
    }
  }

  template <avnd::dynamic_ports_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, std::vector<ossia::value_outlet*>& ports,
      avnd::field_index<Idx>) const noexcept
  {
    ctrl.ports.resize(ports.size());
    for(auto& port_value : ctrl.ports)
    {
      using type = std::remove_cvref_t<decltype(port_value.value)>;
      port_value.value = type{};
    }
  }

  template <avnd::control Field, std::size_t Idx>
    requires(!avnd::sample_accurate_control<Field>)
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    if constexpr(avnd::optional_ish<decltype(Field::value)>)
      ctrl.value = {};
  }

  template <avnd::value_port Field, std::size_t Idx>
    requires(!avnd::sample_accurate_value_port<Field>)
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    if constexpr(avnd::optional_ish<decltype(Field::value)>)
      ctrl.value = {};
  }

  template <avnd::sample_accurate_control Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    // FIXME clear
  }
  template <avnd::sample_accurate_value_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    // FIXME clear
  }
  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::raw_container_midi_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::midi_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::dynamic_container_midi_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::midi_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    ctrl.midi_messages.clear();
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

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::geometry_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }
};

}
