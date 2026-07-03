#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/processor.hpp>
#include <avnd/concepts/worker.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_storage.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/prepare.hpp>
#include <avnd/wrappers/process_adapter.hpp>

#include <pybind11/numpy.h>

#include <vector>

namespace python
{
namespace py = pybind11;

// Drive an audio processor over one block. `ins` is (channels, frames) float32
// (1-D is treated as mono); returns (out_channels, frames). Current control
// values on `self` are copied into the processing instances first.
template <typename T>
py::array_t<float> run_audio(
    T& self, py::array_t<float, py::array::c_style | py::array::forcecast> ins,
    double rate)
{
  const auto info = ins.request();
  int n_in = 0, frames = 0;
  if(info.ndim == 1)
  {
    n_in = 1;
    frames = static_cast<int>(info.shape[0]);
  }
  else if(info.ndim == 2)
  {
    n_in = static_cast<int>(info.shape[0]);
    frames = static_cast<int>(info.shape[1]);
  }
  else
  {
    throw std::runtime_error(
        "process_audio expects a 1-D or 2-D float array (channels, frames)");
  }

  // Output channel count. For monophonic processors (mono per-sample /
  // per-channel, arg OR sample-PORT form) the process adapter is driven by the
  // *input* channel count -- it processes `in.size()` channels and writes that
  // many output channels (the mono per-sample-port adapter even asserts
  // input_channels == output_channels). So the output buffer MUST have as many
  // channels as the input; sizing it by output_channels<T>() (which for a
  // sample-PORT object is the static count of output sample ports, e.g. 1)
  // leaves out_ptrs too small and the adapter writes past it -> heap
  // corruption. Non-monophonic objects keep the introspected/declared count.
  const int n_out = avnd::monophonic_audio_processor<T>
                        ? std::max(1, n_in)
                        : std::max(1, avnd::output_channels<T>(n_in));

  avnd::effect_container<T> c;
  c.init_channels(n_in, n_out);

  // Initialise the container's control storage to the declared defaults BEFORE
  // copying the instance's values over it. This matters for objects whose ports
  // are declared as nested `inputs`/`outputs` TYPES (args-style operator()):
  // their controls live in the container's own storage, not on the bare `T`
  // instance, so the copy below cannot reach them -- without this they would be
  // read uninitialised (e.g. a garbage gain -> constant saturated output). The
  // golden generator does the same (avnd::init_controls before running).
  avnd::init_controls(c);

  for(auto&& st : c.full_state())
  {
    if constexpr(std::is_copy_assignable_v<T>)
      st.effect = self;
    else if constexpr(requires { st.inputs = avnd::get_inputs(self); })
      st.inputs = avnd::get_inputs(self);
  }

  // Host-provided callables must never be empty std::functions when
  // prepare()/process() runs them -- install no-op (or inline, for the
  // worker) handlers, the same set the example host and the golden
  // generator wire.
  if constexpr(avnd::audio_bus_input_introspection<T>::size > 0)
    avnd::audio_bus_input_introspection<T>::for_all(
        avnd::get_inputs(c), [](auto& p) {
          if constexpr(requires { p.request_channels; })
            if(!p.request_channels)
              p.request_channels = [](int) {};
        });
  if constexpr(avnd::audio_bus_output_introspection<T>::size > 0)
    avnd::audio_bus_output_introspection<T>::for_all(
        avnd::get_outputs(c), [](auto& p) {
          if constexpr(requires { p.request_channels; })
            if(!p.request_channels)
              p.request_channels = [](int) {};
        });
  auto wire_buffer = [](auto& p) {
    if constexpr(requires { p.buffer.upload; })
      if(!p.buffer.upload)
        p.buffer.upload = [](const char*, std::int64_t, std::int64_t) {};
  };
  if constexpr(avnd::buffer_output_introspection<T>::size > 0)
    avnd::buffer_output_introspection<T>::for_all(avnd::get_outputs(c), wire_buffer);
  if constexpr(avnd::buffer_input_introspection<T>::size > 0)
    avnd::buffer_input_introspection<T>::for_all(avnd::get_inputs(c), wire_buffer);
  if constexpr(avnd::has_worker<T>)
    for(auto& impl : c.effects())
    {
      using worker_t = std::decay_t<decltype(impl.worker)>;
      impl.worker.request = [](auto&&... args) {
        worker_t::work(std::forward<decltype(args)>(args)...);
      };
    }

  avnd::process_setup su{
      .input_channels = n_in,
      .output_channels = n_out,
      .frames_per_buffer = frames,
      .rate = rate};
  avnd::process_adapter<T> proc;
  proc.allocate_buffers(su, float{});
  proc.allocate_buffers(su, double{});
  avnd::prepare(c, su);

  // Sample-accurate control ports need their per-buffer storage reserved
  // before process() (else the object writes into unallocated storage).
  avnd::control_storage<T> control_buffers;
  if constexpr(sizeof(control_buffers) > 1)
    control_buffers.reserve_space(c, frames);

  auto* in_base = static_cast<float*>(info.ptr);
  std::vector<float*> in_ptrs(n_in);
  for(int ch = 0; ch < n_in; ch++)
    in_ptrs[ch] = in_base + static_cast<std::size_t>(ch) * frames;

  py::array_t<float> out(std::vector<py::ssize_t>{n_out, frames});
  auto out_info = out.request();
  auto* out_base = static_cast<float*>(out_info.ptr);
  std::vector<float*> out_ptrs(n_out);
  for(int ch = 0; ch < n_out; ch++)
    out_ptrs[ch] = out_base + static_cast<std::size_t>(ch) * frames;

  struct tick
  {
    int frames;
  } t{frames};

  proc.process(
      c, avnd::span<float*>{in_ptrs.data(), static_cast<std::size_t>(n_in)},
      avnd::span<float*>{out_ptrs.data(), static_cast<std::size_t>(n_out)}, t);

  // Copy the processed state back so output controls (e.g. analysis results)
  // are readable on the Python instance afterwards.
  for(auto&& st : c.full_state())
  {
    if constexpr(std::is_copy_assignable_v<T>)
      self = st.effect;
    else if constexpr(requires { avnd::get_outputs(self) = st.outputs; })
      avnd::get_outputs(self) = st.outputs;
    break;
  }

  return out;
}
}
