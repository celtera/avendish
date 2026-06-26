#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/channels.hpp>
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

  const int n_out = std::max(1, avnd::output_channels<T>(n_in));

  avnd::effect_container<T> c;
  c.init_channels(n_in, n_out);
  for(auto&& st : c.full_state())
  {
    if constexpr(std::is_copy_assignable_v<T>)
      st.effect = self;
    else if constexpr(requires { st.inputs = avnd::get_inputs(self); })
      st.inputs = avnd::get_inputs(self);
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

  return out;
}
}
