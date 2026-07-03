#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstdint>

namespace avnd::soft_ui
{
// Non-owning view over an RGBA8 pixel buffer. This is the only currency
// between the soft runtime and its shells (pugl blit, canvas putImageData,
// esp_lcd flush, golden tests...).
struct framebuffer
{
  unsigned char* data{};
  int width{};
  int height{};
  int stride{}; // bytes per row; 0 means width * 4

  int row_bytes() const noexcept { return stride > 0 ? stride : width * 4; }
};
}
