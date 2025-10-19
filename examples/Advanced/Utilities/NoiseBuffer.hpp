#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <algorithm>
#include <random>

namespace uo
{
// First version, using a raw buffer output (buffer type is unsigned char*)
struct NoiseBuffer_raw
{
public:
  halp_meta(name, "Noise buffer")
  halp_meta(c_name, "avnd_noisebuffer")
  halp_meta(category, "Visuals")
  halp_meta(description, "Generate a noise buffer")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/noise_buffer.html")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "290d895e-2812-4b2e-8432-d210aac6c28e")

  struct
  {
    halp::spinbox_i32<"Particles", halp::range{1, 1000000, 10000}> count;
  } inputs;

  struct
  {
    halp::buffer_output<"Output"> buffer;
  } outputs;

  void operator()() noexcept
  {
    auto points = outputs.buffer.create<float>(inputs.count * 3);

    std::generate(points.begin(), points.end(), [] {
      static thread_local std::uniform_real_distribution<float> distr{-1.f, 1.f};
      static thread_local std::random_device device;
      static thread_local std::mt19937 engine {device()};
      return distr(engine);
    });

    outputs.buffer.upload();
  }
};

// Second version, using a typed buffer output (buffer type is float*)
// This usually allows for optimizations and better knowledge of data types on the backend
struct NoiseBuffer
{
public:
  halp_meta(name, "Noise buffer")
  halp_meta(c_name, "avnd_noisebuffer")
  halp_meta(category, "Visuals")
  halp_meta(description, "Generate a noise buffer")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/noise_buffer.html")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "290d895e-2812-4b2e-8432-d210aac6c28e")

  struct
  {
    halp::spinbox_i32<"Particles", halp::range{1, 1000000, 10000}> count;
  } inputs;

  struct
  {
    halp::typed_buffer_output<"Output", float> buffer;
  } outputs;

  void operator()() noexcept
  {
    auto points = outputs.buffer.create(inputs.count * 3);

    std::generate(points.begin(), points.end(), [] {
      static thread_local std::uniform_real_distribution<float> distr{-1.f, 1.f};
      static thread_local std::random_device device;
      static thread_local std::mt19937 engine {device()};
      return distr(engine);
    });

    outputs.buffer.upload();
  }
};
}
