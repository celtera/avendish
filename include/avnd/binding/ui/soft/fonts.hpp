#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace avnd::soft_ui
{
// Named TTF store shared by the Nuklear renderer and soft_painter.
// avnd::painter's set_font takes a name; canvas_ity wants TTF bytes --
// this is the bridge. The first registered font is the default.
struct font_registry
{
  struct font
  {
    std::string name;
    std::vector<unsigned char> bytes;
  };
  std::vector<font> fonts;

  void register_font(std::string_view name, std::vector<unsigned char> bytes)
  {
    fonts.push_back({std::string{name}, std::move(bytes)});
  }

  bool register_font_file(std::string_view name, const char* path)
  {
    std::FILE* f = std::fopen(path, "rb");
    if(!f)
      return false;
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if(sz <= 0)
    {
      std::fclose(f);
      return false;
    }
    std::vector<unsigned char> bytes(sz);
    const auto read = std::fread(bytes.data(), 1, sz, f);
    std::fclose(f);
    if(read != static_cast<size_t>(sz))
      return false;
    register_font(name, std::move(bytes));
    return true;
  }

  const font* find(std::string_view name) const noexcept
  {
    for(auto& f : fonts)
      if(f.name == name)
        return &f;
    return fonts.empty() ? nullptr : &fonts.front();
  }

  const font* default_font() const noexcept
  {
    return fonts.empty() ? nullptr : &fonts.front();
  }
};

// Best-effort default font for plug-in editors: build-time override first,
// then well-known system font locations.
inline font_registry system_fonts()
{
  font_registry fonts;
#if defined(AVND_SOFT_UI_DEFAULT_FONT)
  if(fonts.register_font_file("default", AVND_SOFT_UI_DEFAULT_FONT))
    return fonts;
#endif
  static constexpr const char* candidates[] = {
#if defined(__EMSCRIPTEN__)
      // Embedded into the module's virtual FS by avnd_target_soft_ui
      "/font.ttf",
#elif defined(_WIN32)
      "C:\\Windows\\Fonts\\segoeui.ttf",
      "C:\\Windows\\Fonts\\arial.ttf",
      "C:\\Windows\\Fonts\\tahoma.ttf",
#elif defined(__APPLE__)
      "/Library/Fonts/Arial.ttf",
      "/System/Library/Fonts/Supplemental/Arial.ttf",
#else
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/local/share/fonts/dejavu/DejaVuSans.ttf",
#endif
  };
  for(auto path : candidates)
    if(fonts.register_font_file("default", path))
      break;
  return fonts;
}
}
