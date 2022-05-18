#pragma once
#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/concepts/soundfile.hpp>
#include <avnd/concepts/fft.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/audio/fft.hpp>
#include <fmt/printf.h>
#include <ossia/network/value/format_value.hpp>
namespace avnd
{
}

namespace oscr
{

template<typename Field>
inline void update_value(auto& obj, Field& field, const ossia::value& src, auto& dst)
{
  from_ossia_value(field, src, dst);
  if constexpr(requires { &Field::update; }) {
    field.update(obj);
  }
}

template <typename Exec_T, typename Obj_T>
struct process_before_run
{
  Exec_T& self;
  Obj_T& impl;

  template <avnd::parameter Field, std::size_t Idx>
  requires(!avnd::control<Field>) void init_value(
      Field& ctrl,
      ossia::value_inlet& port,
      avnd::field_index<Idx>) const noexcept
  {
    if (!port.data.get_data().empty())
    {
      auto& last = port.data.get_data().back().value;
      update_value(impl, ctrl, last, ctrl.value);
    }
  }

  template <avnd::parameter Field, std::size_t Idx>
  requires(avnd::control<Field>) void init_value(
      Field& ctrl,
      ossia::value_inlet& port,
      avnd::field_index<Idx> idx) const noexcept
  {
    if (!port.data.get_data().empty())
    {
      auto& last = port.data.get_data().back().value;
      update_value(impl, ctrl, last, ctrl.value);

      // Get the index of the control in [0; N[
      using type = typename Exec_T::processor_type;
      using controls = avnd::control_input_introspection<type>;
      constexpr int control_index = controls::field_index_to_index(idx);

      // Mark the control as changed
      self.control.inputs_set.set(control_index);
    }
  }

  template <avnd::parameter Field, std::size_t Idx>
  requires(!avnd::sample_accurate_parameter<Field>) void
  operator()(Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    init_value(ctrl, port, avnd::field_index<Idx>{});
  }

  template <avnd::linear_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    // FIXME we need to know about the buffer size !
    init_value(ctrl, port, avnd::field_index<Idx>{});

    for (auto& [val, ts] : port->get_data())
    {
      auto& v = ctrl.values[ts].emplace();
      update_value(impl, ctrl, val, v);
    }
  }

  template <avnd::span_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    init_value(ctrl, port, avnd::field_index<Idx>{});
    for (auto& [val, ts] : port->get_data())
    {
      // FIXME
    }
  }

  template <avnd::dynamic_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    init_value(ctrl, port, avnd::field_index<Idx>{});
    for (auto& [val, ts] : port->get_data())
    {
      update_value(impl, ctrl, val, ctrl.values[ts]);
    }
  }

  template <avnd::spectrum_split_channel_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    if(port.data.channels() >= 1)
    {
      using sc_in = avnd::spectrum_split_channel_input_introspection<typename Exec_T::processor_type>;
      constexpr auto fft_idx = sc_in::field_index_to_index(idx);
      ossia::fft& fft = get<fft_idx>(self.spectrums.split_channel.ffts);

      auto& samples = port.data.channel(0);
      const int N = samples.size();
      if(N > 0)
      {
        auto fftIn = fft.input();
        for (int i = 0; i < N; i++)
          fftIn[i] = samples[i];

        auto fftOut = reinterpret_cast<ossia::fft_real*>(fft.execute());
        deinterleave(fftOut, N);

        ctrl.spectrum.amplitude = fftOut;
        ctrl.spectrum.phase = fftOut + N / 2;
      }
    }
  }

  template <avnd::spectrum_complex_channel_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    if(port.data.channels() >= 1)
    {
      using sc_in = avnd::spectrum_complex_channel_input_introspection<typename Exec_T::processor_type>;
      constexpr auto fft_idx = sc_in::field_index_to_index(idx);
      ossia::fft& fft = get<fft_idx>(self.spectrums.complex_channel.ffts);

      auto& samples = port.data.channel(0);
      const int N = samples.size();
      if(N > 0)
      {
        auto fftIn = fft.input();
        for (int i = 0; i < N; i++)
          fftIn[i] = samples[i];

        ctrl.spectrum = reinterpret_cast<decltype(ctrl.spectrum)>(fft.execute());
      }
    }
  }

  template <typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::raw_container_midi_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    // FIXME
  }

  template <typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::texture_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::soundfile_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    auto& dat = port.data.get_data();
    if(dat.empty())
      return;

    auto& back = dat.back();
    auto str = back.value.template target<std::string>();
    if(!str)
      return;

    self.soundfile_load_request(*str, Idx);
  }

  template <avnd::dynamic_container_midi_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    ctrl.midi_messages.reserve(port.data.messages.size());
    for (const libremidi::message& msg_in : port.data.messages)
    {
      ctrl.midi_messages.push_back(
          {.bytes{msg_in.begin(), msg_in.end()}, .timestamp{(int)msg_in.timestamp}});
    }
  }

  template <avnd::control Field, std::size_t Idx>
  requires(!avnd::sample_accurate_control<Field>) void
  operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }
  template <avnd::value_port Field, std::size_t Idx>
  requires(!avnd::sample_accurate_value_port<Field>) void
  operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }
  template <avnd::sample_accurate_control Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }
  template <avnd::sample_accurate_value_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }
  template <typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::audio_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <typename Field, std::size_t Idx>
  void
  operator()(Field& ctrl, ossia::texture_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::callback Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }
};

}
