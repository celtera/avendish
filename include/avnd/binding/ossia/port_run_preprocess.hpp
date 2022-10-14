#pragma once
#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/geometry.hpp>
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
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/network/value/format_value.hpp>

namespace oscr
{
struct hanning_window
{
  template <typename T>
  void operator()(const T* in, T* out, int size)
  {
    const T norm = 0.5 / size;
    const auto factor = ossia::two_pi / (size - 1);
    for(int i = 0; i < size; ++i)
    {
      out[i] = in[i] * norm * (1. - std::cos(factor * i));
    }
  }
};

struct hamming_window
{
  template <typename T>
  void operator()(const T* in, T* out, int size)
  {
    constexpr T a0 = 25. / 46.;
    constexpr T a1 = 21. / 46.;

    const T norm = 1. / size;
    const auto factor = ossia::two_pi / (size - 1);
    for(int i = 0; i < size; ++i)
    {
      out[i] = in[i] * norm * (a0 - a1 * std::cos(factor * i));
    }
  }
};

struct apply_window
{
  template <typename Field, typename T>
  void operator()(Field& f, const T* in, T* out, int frames) noexcept
  {
    if constexpr(requires { Field::window::hanning; })
    {
      hanning_window{}(in, out, frames);
    }
    else if constexpr(requires { Field::window::hamming; })
    {
      hamming_window{}(in, out, frames);
    }
    else
    {
      const T mult = 1. / frames;
      for(int i = 0; i < frames; ++i)
      {
        out[i] = mult * in[i];
      }
    }
  }
};

template <typename Field, std::size_t Idx>
inline void update_value(auto& node, auto& obj, Field& field, const ossia::value& src, auto& dst, avnd::field_index<Idx> idx)
{
  node.from_ossia_value(field, src, dst, idx);
  if constexpr(requires { field.update(obj); })
  {
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
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    if(!port.data.get_data().empty())
    {
      auto& last = port.data.get_data().back().value;
      update_value(self, impl, ctrl, last, ctrl.value, idx);
    }
    else if(port.cables().empty())
    {
      if(auto node_ptr = port.address.target<ossia::net::node_base*>())
      {
        ossia::net::node_base& node = **node_ptr;
      }
      else if(auto path_ptr = port.address.target<ossia::traversal::path>())
      {
        ossia::traversal::path& p = *path_ptr;

      }
    }
  }

  template <avnd::parameter Field, std::size_t Idx>
  requires(avnd::control<Field>) void init_value(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    if(!port.data.get_data().empty())
    {
      auto& last = port.data.get_data().back().value;
      update_value(self, impl, ctrl, last, ctrl.value, idx);

      // Get the index of the control in [0; N[
      using type = typename Exec_T::processor_type;
      using controls = avnd::control_input_introspection<type>;
      constexpr int control_index = controls::field_index_to_index(idx);

      // Mark the control as changed
      self.control.inputs_set.set(control_index);
    }
  }

  template <avnd::parameter Field, std::size_t Idx>
  requires(!avnd::sample_accurate_parameter<Field>) void operator()(
      Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx> idx) const noexcept
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
      auto& v = ctrl.values[ts].emplace();
      update_value(self, impl, ctrl, val, v, avnd::field_index<Idx>{});
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
      update_value(self, impl, ctrl, val, ctrl.values[ts], avnd::field_index<Idx>{});
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
      constexpr auto fft_idx = sc_in::field_index_to_index(idx);
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
      constexpr auto fft_idx = sc_in::field_index_to_index(idx);
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
      constexpr auto fft_idx = sc_in::field_index_to_index(idx);
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
      constexpr auto fft_idx = sc_in::field_index_to_index(idx);
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

  template <typename Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::geometry_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    if(port.data.dirty_meshes)
      ctrl.dirty_mesh = true;
    if(port.data.dirty_transform)
      ctrl.dirty_transform = true;
    geometry_from_ossia(port.data.meshes, ctrl);
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

    self.soundfile_load_request(*str, Idx);
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
    ctrl.midi_messages.reserve(port.data.messages.size());
    for(const libremidi::message& msg_in : port.data.messages)
    {
      ctrl.midi_messages.push_back(
          {.bytes{msg_in.begin(), msg_in.end()}, .timestamp{(int)msg_in.timestamp}});
    }
  }


  template <avnd::control Field, std::size_t Idx>
  requires(!avnd::sample_accurate_control<Field>) void operator()(
      Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    if constexpr(avnd::optional_ish<decltype(Field::value)>)
      ctrl.value = {};
  }
  template <avnd::value_port Field, std::size_t Idx>
  requires(!avnd::sample_accurate_value_port<Field>) void operator()(
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
