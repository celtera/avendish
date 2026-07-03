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
}
