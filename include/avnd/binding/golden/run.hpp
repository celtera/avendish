#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Golden-output generator: runs an avendish object offline with deterministic
// test input and writes golden/<c_name>.json (inputs + outputs + meta). The
// file is the oracle every backend binding must reproduce -- see
// OUTPUT_VERIFICATION_PLAN.md. Mirrors what a real binding does (the example
// host in binding/example/example_processor.hpp is the reference), kept to the
// subset needed to feed inputs and read outputs.

#include <avnd/binding/dump/json_writer.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/wrappers/audio_channel_manager.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_storage.hpp>
#include <avnd/wrappers/prepare.hpp>
#include <avnd/wrappers/process_adapter.hpp>

#include <cmath>
#include <cstdint>
#include <deque>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace golden
{
inline constexpr int k_seed = 1234;
inline constexpr int k_frames = 64;
inline constexpr double k_rate = 44100.0;

// Deterministic test signal for input audio channel `ch`, sample `i`.
inline float test_sample(int ch, int i, int frames)
{
  // Bounded, distinct per channel, no randomness.
  return 0.5f * std::sin(2.0 * 3.14159265358979 * (ch + 1) * (i + 1) / frames);
}

// Bytes per pixel for a halp texture (static member or method, else assume 4).
int tex_bpp(const auto& tex)
{
  using Tex = std::decay_t<decltype(tex)>;
  if constexpr(requires { tex.bytes_per_pixel(); })
    return tex.bytes_per_pixel();
  else if constexpr(requires { Tex::bytes_per_pixel; })
    return Tex::bytes_per_pixel;
  else
    return 4;
}

// A clean, matchable name: the port's own name if it has one, else a positional
// key (unnamed ports have no reflected name on compilers without std reflection,
// and get_name<F>() then yields a compiler-mangled placeholder).
template <typename F>
std::string clean_name(int index)
{
  std::string_view nm = avnd::get_name<F>();
  if(nm.empty() || nm.find('<') != std::string_view::npos
     || nm.find("unnamed") != std::string_view::npos)
    return "p" + std::to_string(index);
  return std::string{nm};
}

// Set a deterministic value on an input control field, then append
// {index, name, value} to the ordered `arr` (order matches the dump).
template <typename F>
void set_and_record_control(F& field, dump_json::document& jdoc, dump_json::value arr, int index)
{
  using vt = std::decay_t<decltype(field.value)>;
  auto node = jdoc.make_node();
  node["index"] = index;
  node["name"] = clean_name<F>(index);
  if constexpr(std::is_same_v<vt, bool>)
  {
    field.value = true;
    node["value"] = true;
  }
  else if constexpr(std::floating_point<vt>)
  {
    if constexpr(avnd::parameter_with_minmax_range<F>)
    {
      constexpr auto r = avnd::get_range<F>();
      field.value = static_cast<vt>(r.min + 0.5 * (r.max - r.min));
    }
    else
      field.value = static_cast<vt>(0.5);
    node["value"] = field.value;
  }
  else if constexpr(std::is_enum_v<vt>)
  {
    field.value = static_cast<vt>(0);
    node["value"] = static_cast<int>(field.value);
  }
  else if constexpr(std::integral<vt>)
  {
    if constexpr(avnd::parameter_with_minmax_range<F>)
    {
      constexpr auto r = avnd::get_range<F>();
      field.value = static_cast<vt>(r.min + (r.max - r.min) / 2);
    }
    else
      field.value = static_cast<vt>(1);
    node["value"] = field.value;
  }
  else if constexpr(std::convertible_to<vt, std::string_view>)
  {
    field.value = "test";
    node["value"] = std::string_view{field.value};
  }
  else
  {
    node["value"] = "unrecorded"; // container/struct: structural compare only
  }
  arr.push_back(node);
}

// Append an output control field's value as {index, name, value}.
template <typename F>
void record_output_control(F& field, dump_json::document& jdoc, dump_json::value arr, int index)
{
  using vt = std::decay_t<decltype(field.value)>;
  auto node = jdoc.make_node();
  node["index"] = index;
  node["name"] = clean_name<F>(index);
  if constexpr(std::is_same_v<vt, bool> || std::floating_point<vt> || std::integral<vt>)
    node["value"] = field.value;
  else if constexpr(std::is_enum_v<vt>)
    node["value"] = static_cast<int>(field.value);
  else if constexpr(std::convertible_to<vt, std::string_view>)
    node["value"] = std::string_view{field.value};
  else
    node["value"] = "unrecorded";
  arr.push_back(node);
}

template <typename T>
int run(std::string_view path)
{
  dump_json::document jdoc;
  auto root = jdoc.root();
  root["c_name"] = avnd::get_c_name<T>();
  auto meta = root["meta"];
  meta["seed"] = k_seed;
  meta["frames"] = k_frames;
  meta["rate"] = k_rate;

  auto write = [&](std::string_view status) {
    root["meta"]["status"] = status;
    std::ofstream f{std::string{path}};
    f << jdoc.dump();
    return 0;
  };

  try
  {
    avnd::effect_container<T> effect;
    avnd::process_adapter<T> processor;
    avnd::audio_channel_manager<T> channels{effect};

    // Request a small, fixed channel layout (fixed-channel objects override it).
    channels.set_input_channels(effect, 0, 2);
    channels.set_output_channels(effect, 0, 2);
    avnd::init_controls(effect);

    // Install no-op handlers for host-provided callables so prepare()/process()
    // never call an empty std::function (that is a hard crash). Same set the
    // example host wires.
    if constexpr(avnd::audio_bus_input_introspection<T>::size > 0)
      avnd::audio_bus_input_introspection<T>::for_all(
          avnd::get_inputs(effect), [](auto& p) {
            if constexpr(requires { p.request_channels; })
              if(!p.request_channels)
                p.request_channels = [](int) {};
          });
    if constexpr(avnd::audio_bus_output_introspection<T>::size > 0)
      avnd::audio_bus_output_introspection<T>::for_all(
          avnd::get_outputs(effect), [](auto& p) {
            if constexpr(requires { p.request_channels; })
              if(!p.request_channels)
                p.request_channels = [](int) {};
          });

    // Output/input buffer ports push through buffer.upload -- give it a no-op.
    auto wire_buffer = [](auto& p) {
      if constexpr(requires { p.buffer.upload; })
        if(!p.buffer.upload)
          p.buffer.upload = [](const char*, std::int64_t, std::int64_t) {};
    };
    if constexpr(avnd::buffer_output_introspection<T>::size > 0)
      avnd::buffer_output_introspection<T>::for_all(avnd::get_outputs(effect), wire_buffer);
    if constexpr(avnd::buffer_input_introspection<T>::size > 0)
      avnd::buffer_input_introspection<T>::for_all(avnd::get_inputs(effect), wire_buffer);

    // Workers offload a job to the host; run it inline.
    if constexpr(avnd::has_worker<T>)
      for(auto& impl : effect.effects())
      {
        using worker_t = std::decay_t<decltype(impl.worker)>;
        impl.worker.request = [](auto&&... args) {
          worker_t::work(std::forward<decltype(args)>(args)...);
        };
      }

    const int in_N = channels.actual_runtime_inputs;
    const int out_N = channels.actual_runtime_outputs;

    avnd::process_setup setup{
        .input_channels = in_N,
        .output_channels = out_N,
        .frames_per_buffer = k_frames,
        .rate = k_rate};
    processor.allocate_buffers(setup, float{});
    effect.init_channels(in_N, out_N);
    avnd::prepare(effect, setup);

    // Sample-accurate control ports need their per-buffer storage reserved
    // before process() (else the object writes into unallocated storage).
    avnd::control_storage<T> control_buffers;
    if constexpr(sizeof(control_buffers) > 1)
      control_buffers.reserve_space(effect, k_frames);

    // --- inputs ---
    auto in = root["inputs"];
    auto in_controls = in["controls"];
    in_controls.ensure_array();
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      int idx = 0;
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(effect), [&](auto& field) {
            set_and_record_control(field, jdoc, in_controls, idx++);
          });
    }

    // audio input buffers
    std::vector<std::vector<float>> inbuf(in_N, std::vector<float>(k_frames, 0.f));
    std::vector<std::vector<float>> outbuf(out_N, std::vector<float>(k_frames, 0.f));
    std::vector<float*> in_ptrs(in_N), out_ptrs(out_N);
    auto in_audio = in["audio"];
    in_audio.ensure_array();
    for(int c = 0; c < in_N; ++c)
    {
      for(int i = 0; i < k_frames; ++i)
        inbuf[c][i] = test_sample(c, i, k_frames);
      in_ptrs[c] = inbuf[c].data();
      auto chnode = jdoc.make_node();
      chnode = inbuf[c];
      in_audio.push_back(chnode);
    }
    for(int c = 0; c < out_N; ++c)
      out_ptrs[c] = outbuf[c].data();

    // CPU texture inputs: give each a small deterministic test image so filters
    // have something to read (storage must outlive process()). GPU textures are
    // not handled (they need a graphics context).
    std::deque<std::vector<unsigned char>> tex_storage;
    if constexpr(avnd::cpu_texture_input_introspection<T>::size > 0)
      avnd::cpu_texture_input_introspection<T>::for_all(
          avnd::get_inputs(effect), [&](auto& port) {
            const int w = 16, h = 16, bpp = tex_bpp(port.texture);
            auto& buf = tex_storage.emplace_back((std::size_t)w * h * bpp);
            for(std::size_t i = 0; i < buf.size(); ++i)
              buf[i] = (unsigned char)(i & 0xFF);
            port.texture.width = w;
            port.texture.height = h;
            port.texture.bytes
                = reinterpret_cast<decltype(port.texture.bytes)>(buf.data());
            if constexpr(requires { port.texture.changed; })
              port.texture.changed = true;
          });

    // --- run ---
    processor.process(
        effect, avnd::span<float*>{in_ptrs.data(), (std::size_t)in_N},
        avnd::span<float*>{out_ptrs.data(), (std::size_t)out_N}, k_frames);

    // --- outputs ---
    auto out = root["outputs"];
    auto out_controls = out["controls"];
    out_controls.ensure_array();
    if constexpr(avnd::parameter_output_introspection<T>::size > 0)
    {
      int idx = 0;
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(effect), [&](auto& field) {
            record_output_control(field, jdoc, out_controls, idx++);
          });
    }

    auto out_audio = out["audio"];
    out_audio.ensure_array();
    for(int c = 0; c < out_N; ++c)
    {
      auto chnode = jdoc.make_node();
      chnode = outbuf[c];
      out_audio.push_back(chnode);
    }

    // CPU texture outputs: record dimensions + a content hash (exact pixels are
    // large; the hash is enough for a backend to match against).
    auto out_tex = out["texture"];
    out_tex.ensure_array();
    if constexpr(avnd::cpu_texture_output_introspection<T>::size > 0)
    {
      int tidx = 0;
      avnd::cpu_texture_output_introspection<T>::for_all(
          avnd::get_outputs(effect), [&](auto& port) {
            auto node = jdoc.make_node();
            node["index"] = tidx++;
            node["width"] = port.texture.width;
            node["height"] = port.texture.height;
            std::uint64_t hash = 1469598103934665603ULL;
            const int bpp = tex_bpp(port.texture);
            if(port.texture.bytes && port.texture.width > 0 && port.texture.height > 0)
            {
              const std::size_t n
                  = (std::size_t)port.texture.width * port.texture.height * bpp;
              const auto* b = reinterpret_cast<const unsigned char*>(port.texture.bytes);
              for(std::size_t i = 0; i < n; ++i)
              {
                hash ^= b[i];
                hash *= 1099511628211ULL;
              }
            }
            node["hash"] = (std::int64_t)hash;
            out_tex.push_back(node);
          });
    }

    return write("ok");
  }
  catch(const std::exception& e)
  {
    root["meta"]["error"] = std::string_view{e.what()};
    return write("error");
  }
  catch(...)
  {
    return write("error");
  }
}
}
