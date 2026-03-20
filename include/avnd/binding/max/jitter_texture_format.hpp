#pragma once
#include <jit.common.h>
#include <max.jit.mop.h>
#include <avnd/concepts/gfx.hpp>
#include <boost/container/vector.hpp>

#include <algorithm>
#include <type_traits>
#include <string_view>

namespace max::jitter
{
struct max_texture_spec {
  enum class FormatEnum {
    ARGB8        ,
    RGBA8        ,
    BGRA8        ,
    R8           ,
    RG8          ,
    R16          ,
    RG16         ,
    RED_OR_ALPHA8,
    RGBA16F      ,
    RGBA32F      ,
    ARGB32F      ,
    R16F         ,
    R32F         ,
    RGB10A2      ,
    D16          ,
    D24          ,
    D24S8        ,
    D32F         ,
    RGB8         ,
  };

  struct Format {
    int planes = 1;
    t_symbol* type = _jit_sym_char;
    FormatEnum format{};
    Format() = delete;
    Format(const Format&) = delete;
    Format(Format&&) = delete;
    Format(int p , t_symbol* s, FormatEnum e): planes{p}, type{s}, format{e} {}
  };

  static const inline Format ARGB8         { 4, _jit_sym_char   , FormatEnum::ARGB8         };
  static const inline Format RGBA8         { 4, _jit_sym_char   , FormatEnum::RGBA8         };
  static const inline Format BGRA8         { 4, _jit_sym_char   , FormatEnum::BGRA8         };
  static const inline Format R8            { 1, _jit_sym_char   , FormatEnum::R8            };
  static const inline Format RG8           { 2, _jit_sym_char   , FormatEnum::RG8           };
  static const inline Format R16           { 1, _jit_sym_long   , FormatEnum::R16           }; // :(
  static const inline Format RG16          { 2, _jit_sym_long   , FormatEnum::RG16          }; // :(
  static const inline Format RED_OR_ALPHA8 { 1, _jit_sym_char   , FormatEnum::RED_OR_ALPHA8 };
  static const inline Format RGBA16F       { 4, _jit_sym_float32, FormatEnum::RGBA16F       }; // :(
  static const inline Format RGBA32F       { 4, _jit_sym_float32, FormatEnum::RGBA32F       };
  static const inline Format ARGB32F       { 4, _jit_sym_float32, FormatEnum::ARGB32F       };
  static const inline Format R16F          { 1, _jit_sym_float32, FormatEnum::R16F          }; // :(
  static const inline Format R32F          { 1, _jit_sym_float32, FormatEnum::R32F          };
  static const inline Format RGB10A2       { 4, _jit_sym_char   , FormatEnum::RGB10A2       }; // :(
  static const inline Format D16           { 1, _jit_sym_long   , FormatEnum::D16           }; // :(
  static const inline Format D24           { 1, _jit_sym_long   , FormatEnum::D24           }; // :(
  static const inline Format D24S8         { 1, _jit_sym_long   , FormatEnum::D24S8         }; // :(
  static const inline Format D32F          { 1, _jit_sym_float32, FormatEnum::D32F          };
  static const inline Format RGB           { 3, _jit_sym_char   , FormatEnum::RGB8          };
  static const inline Format& RGB8         = RGB;
};

template <typename F>
  requires std::is_enum_v<F>
constexpr const max_texture_spec::Format& texture_spec(F f) noexcept
{
  if constexpr(requires { F::RGBA; } || requires { F::RGBA8; })
    if(f == F::RGBA8)
      return max_texture_spec::RGBA8;
  if constexpr(requires { F::ARGB; } || requires { F::ARGB8; })
    if(f == F::ARGB8)
      return max_texture_spec::ARGB8;
  if constexpr(requires { F::BGRA; } || requires { F::BGRA8; })
    if(f == F::BGRA8)
      return max_texture_spec::BGRA8;
  if constexpr(requires { F::R8; } || requires { F::GRAYSCALE; })
    if(f == F::R8)
      return max_texture_spec::R8;
  if constexpr(requires { F::RG8; })
    if(f == F::RG8)
      return max_texture_spec::RG8;
  if constexpr(requires { F::R16; })
    if(f == F::R16)
      return max_texture_spec::R16;
  if constexpr(requires { F::RG16; })
    if(f == F::RG16)
      return max_texture_spec::RG16;
  if constexpr(requires { F::RED_OR_ALPHA8; })
    if(f == F::RED_OR_ALPHA8)
      return max_texture_spec::RED_OR_ALPHA8;
  if constexpr(requires { F::RGBA16F; })
    if(f == F::RGBA16F)
      return max_texture_spec::RGBA16F;
  if constexpr(requires { F::RGBA32F; })
    if(f == F::RGBA32F)
      return max_texture_spec::RGBA32F;
  if constexpr(requires { F::ARGB32F; })
    if(f == F::ARGB32F)
      return max_texture_spec::ARGB32F;
  if constexpr(requires { F::R16F; })
    if(f == F::R16F)
      return max_texture_spec::R16F;
  if constexpr(requires { F::R32F; })
    if(f == F::R32F)
      return max_texture_spec::R32F;
  if constexpr(requires { F::RGB10A2; })
    if(f == F::RGB10A2)
      return max_texture_spec::RGB10A2;
  if constexpr(requires { F::D16; })
    if(f == F::D16)
      return max_texture_spec::D16;
  if constexpr(requires { F::D24; })
    if(f == F::D24)
      return max_texture_spec::D24;
  if constexpr(requires { F::D24S8; })
    if(f == F::D24S8)
      return max_texture_spec::D24S8;
  if constexpr(requires { F::D32F; })
    if(f == F::D32F)
      return max_texture_spec::D32F;
  if constexpr(requires { F::RGB; })
    if(f == F::RGB)
      return max_texture_spec::RGB;

  return max_texture_spec::RGBA8;
}

template <typename F>
const max_texture_spec::Format& texture_spec() noexcept
{
  if constexpr(requires { std::string_view{F::format()}; })
  {
    constexpr std::string_view fmt = F::format();

    if(fmt == "rgba" || fmt == "rgba8")
      return max_texture_spec::RGBA8;
    if(fmt == "argb" || fmt == "argb8")
      return max_texture_spec::ARGB8;
    else if(fmt == "bgra" || fmt == "bgra8")
      return max_texture_spec::BGRA8;
    else if(fmt == "r8")
      return max_texture_spec::R8;
    else if(fmt == "rg8")
      return max_texture_spec::RG8;
    else if(fmt == "r16")
      return max_texture_spec::R16;
    else if(fmt == "rg16")
      return max_texture_spec::RG16;
    else if(fmt == "red_or_alpha8")
      return max_texture_spec::RED_OR_ALPHA8;
    else if(fmt == "rgba16f")
      return max_texture_spec::RGBA16F;
    else if(fmt == "rgba32f")
      return max_texture_spec::RGBA32F;
    else if(fmt == "argb32f")
      return max_texture_spec::ARGB32F;
    else if(fmt == "r16f")
      return max_texture_spec::R16F;
    else if(fmt == "r32")
      return max_texture_spec::R32F;
    else if(fmt == "rgb10a2")
      return max_texture_spec::RGB10A2;
    else if(fmt == "d16")
      return max_texture_spec::D16;
    else if(fmt == "d24")
      return max_texture_spec::D24;
    else if(fmt == "d24s8")
      return max_texture_spec::D24S8;
    else if(fmt == "d32f")
      return max_texture_spec::D32F;
    else if(fmt == "rgb")
      return max_texture_spec::RGB;
    else
      return max_texture_spec::RGBA8;
  }
  else if constexpr(std::is_enum_v<typename F::format>)
  {
    if constexpr(requires { F::RGBA; } || requires { F::RGBA8; })
      return max_texture_spec::RGBA8;
    if constexpr(requires { F::ARGB; } || requires { F::ARGB8; })
      return max_texture_spec::ARGB8;
    else if constexpr(requires { F::BGRA; } || requires { F::BGRA8; })
      return max_texture_spec::BGRA8;
    else if constexpr(requires { F::R8; } || requires { F::GRAYSCALE; })
      return max_texture_spec::R8;
    else if constexpr(requires { F::RG8; })
      return max_texture_spec::RG8;
    else if constexpr(requires { F::R16; })
      return max_texture_spec::R16;
    else if constexpr(requires { F::RG16; })
      return max_texture_spec::RG16;
    else if constexpr(requires { F::RED_OR_ALPHA8; })
      return max_texture_spec::RED_OR_ALPHA8;
    else if constexpr(requires { F::RGBA16F; })
      return max_texture_spec::RGBA16F;
    else if constexpr(requires { F::RGBA32F; })
      return max_texture_spec::RGBA32F;
    else if constexpr(requires { F::ARGB32F; })
      return max_texture_spec::ARGB32F;
    else if constexpr(requires { F::R16F; })
      return max_texture_spec::R16F;
    else if constexpr(requires { F::R32F; })
      return max_texture_spec::R32F;
    else if constexpr(requires { F::RGB10A2; })
      return max_texture_spec::RGB10A2;
    else if constexpr(requires { F::D16; })
      return max_texture_spec::D16;
    else if constexpr(requires { F::D24; })
      return max_texture_spec::D24;
    else if constexpr(requires { F::D24S8; })
      return max_texture_spec::D24S8;
    else if constexpr(requires { F::D32F; })
      return max_texture_spec::D32F;
    else if constexpr(requires { F::RGB; })
      return max_texture_spec::RGB;
    else
      return max_texture_spec::RGBA8;
  }
}

template <avnd::cpu_texture Tex>
const max_texture_spec::Format& texture_spec(const Tex& t) noexcept
{
  if constexpr(avnd::cpu_dynamic_format_texture<Tex>)
    return texture_spec(t.format);
  else
    return texture_spec<Tex>();
}


inline
    unsigned char* convert_input_texture(
        void* in_bytes, int in_width, int in_height, int in_planes, t_symbol* in_type,
        boost::container::vector<unsigned char>& temp_storage, max_texture_spec::FormatEnum out_type)
{
  const int in_n_pixels = in_width * in_height;
  switch(in_planes)
  {
    default:
    case 0:
      break;

    case 1:
      if(in_type == _jit_sym_char)
      {
        // R8 to max_texture_spec::FormatEnum
        auto src = (uint8_t*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
            return (unsigned char*)in_bytes;

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              auto gray = src[i];
              dst[i * 4 + 0] = gray;
              dst[i * 4 + 1] = gray;
              dst[i * 4 + 2] = gray;
              dst[i * 4 + 3] = 255;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              auto gray = src[i];
              dst[i * 3 + 0] = gray;
              dst[i * 3 + 1] = gray;
              dst[i * 3 + 2] = gray;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              float gray = src[i] / 255.0f;
              dst[i * 4 + 0] = gray;
              dst[i * 4 + 1] = gray;
              dst[i * 4 + 2] = gray;
              dst[i * 4 + 3] = 1.0f;
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      else if(in_type == _jit_sym_long)
      {
        // R32UI to max_texture_spec::FormatEnum
        auto src = (uint32_t*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i] = (uint64_t(src[i]) * 255) / 4294967295UL;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              uint8_t gray = (uint64_t(src[i]) * 255) / 4294967295UL;
              dst[i * 4 + 0] = gray;
              dst[i * 4 + 1] = gray;
              dst[i * 4 + 2] = gray;
              dst[i * 4 + 3] = 255;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              uint8_t gray = (uint64_t(src[i]) * 255) / 4294967295UL;
              dst[i * 3 + 0] = gray;
              dst[i * 3 + 1] = gray;
              dst[i * 3 + 2] = gray;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              float gray = src[i] / 4294967295.0f;
              dst[i * 4 + 0] = gray;
              dst[i * 4 + 1] = gray;
              dst[i * 4 + 2] = gray;
              dst[i * 4 + 3] = 1.0f;
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      else if(in_type == _jit_sym_float32)
      {
        // R32F to max_texture_spec::FormatEnum
        auto src = (float*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i] = std::clamp(int(src[i] * 255.0f), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              uint8_t gray = std::clamp(int(src[i] * 255.0f), 0, 255);
              dst[i * 4 + 0] = gray;
              dst[i * 4 + 1] = gray;
              dst[i * 4 + 2] = gray;
              dst[i * 4 + 3] = 255;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              uint8_t gray = std::clamp(int(src[i] * 255.0f), 0, 255);
              dst[i * 3 + 0] = gray;
              dst[i * 3 + 1] = gray;
              dst[i * 3 + 2] = gray;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              float gray = src[i];
              dst[i * 4 + 0] = gray;
              dst[i * 4 + 1] = gray;
              dst[i * 4 + 2] = gray;
              dst[i * 4 + 3] = 1.0f;
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      else if(in_type == _jit_sym_float64)
      {
        // R64F to max_texture_spec::FormatEnum
        auto src = (double*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i] = std::clamp(int(src[i] * 255.0), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              uint8_t gray = std::clamp(int(src[i] * 255.0), 0, 255);
              dst[i * 4 + 0] = gray;
              dst[i * 4 + 1] = gray;
              dst[i * 4 + 2] = gray;
              dst[i * 4 + 3] = 255;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              uint8_t gray = std::clamp(int(src[i] * 255.0), 0, 255);
              dst[i * 3 + 0] = gray;
              dst[i * 3 + 1] = gray;
              dst[i * 3 + 2] = gray;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              float gray = (float)src[i];
              dst[i * 4 + 0] = gray;
              dst[i * 4 + 1] = gray;
              dst[i * 4 + 2] = gray;
              dst[i * 4 + 3] = 1.0f;
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      break;

    case 3:
      if(in_type == _jit_sym_char)
      {
        // RGB8 to max_texture_spec::FormatEnum
        auto src = (uint8_t*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i] = (src[i * 3 + 0] * 299 + src[i * 3 + 1] * 587 + src[i * 3 + 2] * 114) / 1000;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
            return (unsigned char*)in_bytes;

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = src[i * 3 + 0];
              dst[i * 4 + 1] = src[i * 3 + 1];
              dst[i * 4 + 2] = src[i * 3 + 2];
              dst[i * 4 + 3] = 255;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = src[i * 3 + 0] / 255.0f;
              dst[i * 4 + 1] = src[i * 3 + 1] / 255.0f;
              dst[i * 4 + 2] = src[i * 3 + 2] / 255.0f;
              dst[i * 4 + 3] = 1.0f;
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      else if(in_type == _jit_sym_long)
      {
        // RGB32UI to max_texture_spec::FormatEnum
        auto src = (uint32_t*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              uint64_t gray = (uint64_t(src[i * 3 + 0]) * 299 + uint64_t(src[i * 3 + 1]) * 587 + uint64_t(src[i * 3 + 2]) * 114) / 1000;
              dst[i] = (gray * 255) / 4294967295UL;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 3 + 0] = (uint64_t(src[i * 3 + 0]) * 255) / 4294967295UL;
              dst[i * 3 + 1] = (uint64_t(src[i * 3 + 1]) * 255) / 4294967295UL;
              dst[i * 3 + 2] = (uint64_t(src[i * 3 + 2]) * 255) / 4294967295UL;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = (uint64_t(src[i * 3 + 0]) * 255) / 4294967295UL;
              dst[i * 4 + 1] = (uint64_t(src[i * 3 + 1]) * 255) / 4294967295UL;
              dst[i * 4 + 2] = (uint64_t(src[i * 3 + 2]) * 255) / 4294967295UL;
              dst[i * 4 + 3] = 255;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = src[i * 3 + 0] / 4294967295.0f;
              dst[i * 4 + 1] = src[i * 3 + 1] / 4294967295.0f;
              dst[i * 4 + 2] = src[i * 3 + 2] / 4294967295.0f;
              dst[i * 4 + 3] = 1.0f;
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      else if(in_type == _jit_sym_float32)
      {
        // RGB32F to max_texture_spec::FormatEnum
        auto src = (float*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              float gray = src[i * 3 + 0] * 0.299f + src[i * 3 + 1] * 0.587f + src[i * 3 + 2] * 0.114f;
              dst[i] = std::clamp(int(gray * 255.0f), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 3 + 0] = std::clamp(int(src[i * 3 + 0] * 255.0f), 0, 255);
              dst[i * 3 + 1] = std::clamp(int(src[i * 3 + 1] * 255.0f), 0, 255);
              dst[i * 3 + 2] = std::clamp(int(src[i * 3 + 2] * 255.0f), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = std::clamp(int(src[i * 3 + 0] * 255.0f), 0, 255);
              dst[i * 4 + 1] = std::clamp(int(src[i * 3 + 1] * 255.0f), 0, 255);
              dst[i * 4 + 2] = std::clamp(int(src[i * 3 + 2] * 255.0f), 0, 255);
              dst[i * 4 + 3] = 255;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = src[i * 3 + 0];
              dst[i * 4 + 1] = src[i * 3 + 1];
              dst[i * 4 + 2] = src[i * 3 + 2];
              dst[i * 4 + 3] = 1.0f;
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      else if(in_type == _jit_sym_float64)
      {
        // RGB64F to max_texture_spec::FormatEnum
        auto src = (double*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              double gray = src[i * 3 + 0] * 0.299 + src[i * 3 + 1] * 0.587 + src[i * 3 + 2] * 0.114;
              dst[i] = std::clamp(int(gray * 255.0), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 3 + 0] = std::clamp(int(src[i * 3 + 0] * 255.0), 0, 255);
              dst[i * 3 + 1] = std::clamp(int(src[i * 3 + 1] * 255.0), 0, 255);
              dst[i * 3 + 2] = std::clamp(int(src[i * 3 + 2] * 255.0), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = std::clamp(int(src[i * 3 + 0] * 255.0), 0, 255);
              dst[i * 4 + 1] = std::clamp(int(src[i * 3 + 1] * 255.0), 0, 255);
              dst[i * 4 + 2] = std::clamp(int(src[i * 3 + 2] * 255.0), 0, 255);
              dst[i * 4 + 3] = 255;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = (float)src[i * 3 + 0];
              dst[i * 4 + 1] = (float)src[i * 3 + 1];
              dst[i * 4 + 2] = (float)src[i * 3 + 2];
              dst[i * 4 + 3] = 1.0f;
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      break;

    case 4:
      if(in_type == _jit_sym_char)
      {
        // ARGB8 to max_texture_spec::FormatEnum
        auto src = (uint8_t*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              // ARGB: A, R, G, B
              dst[i] = (src[i * 4 + 1] * 299 + src[i * 4 + 2] * 587 + src[i * 4 + 3] * 114) / 1000;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              // ARGB to RGB
              dst[i * 3 + 0] = src[i * 4 + 1]; // R
              dst[i * 3 + 1] = src[i * 4 + 2]; // G
              dst[i * 3 + 2] = src[i * 4 + 3]; // B
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              // ARGB to RGBA
              dst[i * 4 + 0] = src[i * 4 + 1]; // R
              dst[i * 4 + 1] = src[i * 4 + 2]; // G
              dst[i * 4 + 2] = src[i * 4 + 3]; // B
              dst[i * 4 + 3] = src[i * 4 + 0]; // A
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              // ARGB to RGBA
              dst[i * 4 + 0] = src[i * 4 + 1] / 255.0f; // R
              dst[i * 4 + 1] = src[i * 4 + 2] / 255.0f; // G
              dst[i * 4 + 2] = src[i * 4 + 3] / 255.0f; // B
              dst[i * 4 + 3] = src[i * 4 + 0] / 255.0f; // A
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      else if(in_type == _jit_sym_long)
      {
        // ARGB32UI to max_texture_spec::FormatEnum
        auto src = (uint32_t*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              uint64_t gray = (uint64_t(src[i * 4 + 1]) * 299 + uint64_t(src[i * 4 + 2]) * 587 + uint64_t(src[i * 4 + 3]) * 114) / 1000;
              dst[i] = (gray * 255) / 4294967295UL;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 3 + 0] = (uint64_t(src[i * 4 + 1]) * 255) / 4294967295UL;
              dst[i * 3 + 1] = (uint64_t(src[i * 4 + 2]) * 255) / 4294967295UL;
              dst[i * 3 + 2] = (uint64_t(src[i * 4 + 3]) * 255) / 4294967295UL;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = (uint64_t(src[i * 4 + 1]) * 255) / 4294967295UL;
              dst[i * 4 + 1] = (uint64_t(src[i * 4 + 2]) * 255) / 4294967295UL;
              dst[i * 4 + 2] = (uint64_t(src[i * 4 + 3]) * 255) / 4294967295UL;
              dst[i * 4 + 3] = (uint64_t(src[i * 4 + 0]) * 255) / 4294967295UL;
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = src[i * 4 + 1] / 4294967295.0f;
              dst[i * 4 + 1] = src[i * 4 + 2] / 4294967295.0f;
              dst[i * 4 + 2] = src[i * 4 + 3] / 4294967295.0f;
              dst[i * 4 + 3] = src[i * 4 + 0] / 4294967295.0f;
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      else if(in_type == _jit_sym_float32)
      {
        // ARGB32F to max_texture_spec::FormatEnum
        auto src = (float*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              float gray = src[i * 4 + 1] * 0.299f + src[i * 4 + 2] * 0.587f + src[i * 4 + 3] * 0.114f;
              dst[i] = std::clamp(int(gray * 255.0f), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 3 + 0] = std::clamp(int(src[i * 4 + 1] * 255.0f), 0, 255);
              dst[i * 3 + 1] = std::clamp(int(src[i * 4 + 2] * 255.0f), 0, 255);
              dst[i * 3 + 2] = std::clamp(int(src[i * 4 + 3] * 255.0f), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = std::clamp(int(src[i * 4 + 1] * 255.0f), 0, 255);
              dst[i * 4 + 1] = std::clamp(int(src[i * 4 + 2] * 255.0f), 0, 255);
              dst[i * 4 + 2] = std::clamp(int(src[i * 4 + 3] * 255.0f), 0, 255);
              dst[i * 4 + 3] = std::clamp(int(src[i * 4 + 0] * 255.0f), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              // ARGB to RGBA
              dst[i * 4 + 0] = src[i * 4 + 1];
              dst[i * 4 + 1] = src[i * 4 + 2];
              dst[i * 4 + 2] = src[i * 4 + 3];
              dst[i * 4 + 3] = src[i * 4 + 0];
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      else if(in_type == _jit_sym_float64)
      {
        // ARGB64F to max_texture_spec::FormatEnum
        auto src = (double*)in_bytes;
        switch(out_type) {
          case max_texture_spec::FormatEnum::R8:
          {
            temp_storage.resize(in_n_pixels);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              double gray = src[i * 4 + 1] * 0.299 + src[i * 4 + 2] * 0.587 + src[i * 4 + 3] * 0.114;
              dst[i] = std::clamp(int(gray * 255.0), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGB8:
          {
            temp_storage.resize(in_n_pixels * 3);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 3 + 0] = std::clamp(int(src[i * 4 + 1] * 255.0), 0, 255);
              dst[i * 3 + 1] = std::clamp(int(src[i * 4 + 2] * 255.0), 0, 255);
              dst[i * 3 + 2] = std::clamp(int(src[i * 4 + 3] * 255.0), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA8:
          {
            temp_storage.resize(in_n_pixels * 4);
            auto dst = temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              dst[i * 4 + 0] = std::clamp(int(src[i * 4 + 1] * 255.0), 0, 255);
              dst[i * 4 + 1] = std::clamp(int(src[i * 4 + 2] * 255.0), 0, 255);
              dst[i * 4 + 2] = std::clamp(int(src[i * 4 + 3] * 255.0), 0, 255);
              dst[i * 4 + 3] = std::clamp(int(src[i * 4 + 0] * 255.0), 0, 255);
            }
            return temp_storage.data();
          }

          case max_texture_spec::FormatEnum::RGBA32F:
          {
            temp_storage.resize(in_n_pixels * 16);
            auto dst = (float*)temp_storage.data();
            for(int i = 0; i < in_n_pixels; i++)
            {
              // ARGB to RGBA
              dst[i * 4 + 0] = (float)src[i * 4 + 1];
              dst[i * 4 + 1] = (float)src[i * 4 + 2];
              dst[i * 4 + 2] = (float)src[i * 4 + 3];
              dst[i * 4 + 3] = (float)src[i * 4 + 0];
            }
            return temp_storage.data();
          }

          default:
            break;
        }
      }
      break;
  }
  return nullptr;
}

inline void copy_texture_to_max(const max_texture_spec::Format& fmt, void* matrix_data, void* tex_bytes, int width, int height, int tex_bytesize)
{
  const int pixel_count = width * height;

  if(fmt.type == _jit_sym_char)
  {
    unsigned char* dst = static_cast<unsigned char*>(matrix_data);
    const unsigned char* src = static_cast<const unsigned char*>(tex_bytes);
    const int bytesize = fmt.planes * sizeof(char);

    if(&fmt == &max_texture_spec::RGBA8)
    {
      for(int i = 0, N = pixel_count * 4; i < N; i += 4) {
        dst[i + 0] = src[i + 3];
        dst[i + 1] = src[i + 0];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i + 2];
      }
    }
    else if(&fmt == &max_texture_spec::BGRA8)
    {
      for(int i = 0, N = pixel_count * 4; i < N; i += 4) {
        dst[i + 0] = src[i + 3];
        dst[i + 1] = src[i + 2];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i + 0];
      }
    }
    else
    {
      std::memcpy(dst, src, std::min(pixel_count * bytesize, tex_bytesize));
    }
  }
  else if(fmt.type == _jit_sym_float32)
  {
    float* dst = static_cast<float*>(matrix_data);
    const float* src = static_cast<const float*>(tex_bytes);
    const int bytesize = fmt.planes * sizeof(float);
    if(&fmt == &max_texture_spec::RGBA32F)
    {
      for(int i = 0, N = pixel_count * 4; i < N; i += 4) {
        dst[i + 0] = src[i + 3];
        dst[i + 1] = src[i + 0];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i + 2];
      }
    }
    else if(&fmt == &max_texture_spec::RGBA32F)
    {
      for(int i = 0, N = pixel_count * 4; i < N; i += 4) {
        dst[i + 0] = src[i + 3];
        dst[i + 1] = src[i + 0];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i + 2];
      }
    }
    else
    {
      std::memcpy(dst, src, std::min(pixel_count * bytesize, tex_bytesize));
    }
  }
}


}
