#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// CPU texture passthrough, shared by the per-format test objects.

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace examples::tests
{
template <typename In, typename Out>
inline void texture_passthrough(In& in_port, Out& out_port)
{
  auto& in = in_port.texture;
  auto& out = out_port.texture;

  // GPU readback is asynchronous: bytes may not be there on the first frames.
  if(in.bytes == nullptr || in.width <= 0 || in.height <= 0)
    return;
  if(!in.changed)
    return;
  in.changed = false;

  if(out.width != in.width || out.height != in.height)
    out_port.create(in.width, in.height);

  if(out.bytes != nullptr)
  {
    using tex_type = std::decay_t<decltype(in)>;
    const std::size_t n = std::size_t(in.width) * in.height * tex_type::bytes_per_pixel;
    std::memcpy(out.bytes, in.bytes, n);
    out_port.upload();
  }
}
}
