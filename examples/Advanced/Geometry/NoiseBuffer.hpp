#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <random>
#if __has_include(<rnd/random.hpp>)
#include <rnd/random.hpp>
#endif

#include <algorithm>
#include <execution>

namespace uo
{
// Third version, direct callback to GPU. Avoids a copy.
// The data buffer being uploaded MUST stay alive until the next time operator()
// is called.
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
    halp::spinbox_i32<"Particles", halp::range{1, 100000000, 10000}> count;
  } inputs;

  struct
  {
    struct buf {
      halp_meta(name, "Buffer")
      struct {
        void* handle;
        int bytesize;
        std::function<void(const char*, int, int)> upload;
      } buffer;
    } buffer;
  } outputs;

  boost::container::vector<float> points;
  void operator()() noexcept
  {
    // Each point is an x y z value.
    points.resize(inputs.count * 3, boost::container::default_init);

    std::generate(std::execution::par_unseq, points.begin(), points.end(), [] {
      static thread_local std::random_device dev;
#if __has_include(<rnd/random.hpp>)
      static thread_local rnd::pcg engine{dev};
#else
      static thread_local std::mt19937 engine{dev()};
#endif
      return (float(engine()) / float(UINT32_MAX));
    });

    outputs.buffer.buffer.upload((const char*)points.data(), 0, points.size() * sizeof(float));
  }
};
}
