#pragma once

#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/process_execution.hpp>
#include <gst/audio/audio.h>
#include <gst/base/gstbasesink.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstpushsrc.h>
#include <gst/gst.h>
#include <gst/video/video.h>

namespace gst
{
inline std::string canonical_name(std::string_view symname)
{
  char canonical[512];
  int i = 0;
  for(; i < std::ssize(symname) && i < 512; i++)
  {
    char c = symname[i];
    if((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
    {
      canonical[i] = c;
    }
    else if(c >= 'A' && c <= 'Z')
    {
      canonical[i] = c + ('a' - 'A');
    }
    else
    {
      canonical[i] = '-';
    }
  }
  if(i < 512)
    canonical[i] = 0;
  else
    canonical[511] = 0;
  return canonical;
}
template <std::size_t N>
inline std::string canonical_name(const std::array<char, N>& symname)
{
  return canonical_name(std::string_view{symname.data(), symname.size()});
}

template <typename F>
  requires(avnd::has_range<F> && avnd::enum_ish_parameter<F>)
static GParamSpec* param_spec()
{
  static constexpr std::string_view name = avnd::get_name<F>();
  static constexpr std::string_view desc = avnd::get_description<F>();

  static const auto canonical = canonical_name(avnd::get_static_symbol<F>());
  using V = decltype(F::value);
  static constexpr auto range = avnd::get_range<F>();
  int flags = G_PARAM_READWRITE;
  if constexpr(std::is_same_v<float, V>)
  {
    return g_param_spec_float(
        canonical.data(), name.data(), desc.data(), 0.f, 1.f, range.init,
        static_cast<GParamFlags>(flags));
  }
  else
  {
    return g_param_spec_string(
        canonical.data(), name.data(), desc.data(), "",
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }
}

template <typename F>
  requires(!avnd::has_range<F>)
static GParamSpec* param_spec()
{
  static constexpr std::string_view name = avnd::get_name<F>();
  static constexpr std::string_view desc = avnd::get_description<F>();
  static const auto canonical = canonical_name(avnd::get_static_symbol<F>());

  using V = decltype(F::value);
  int flags = G_PARAM_READWRITE;
  if constexpr(std::is_same_v<float, V>)
  {
    return g_param_spec_float(
        canonical.data(), name.data(), desc.data(), 0.f, 1.f, 0.f,
        static_cast<GParamFlags>(flags));
  }
  else
  {
    return g_param_spec_string(
        canonical.data(), name.data(), desc.data(), "",
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }
}

template <typename F>
  requires(avnd::has_range<F> && !avnd::enum_ish_parameter<F>)
static GParamSpec* param_spec()
{
  static constexpr std::string_view name = avnd::get_name<F>();
  static constexpr std::string_view desc = avnd::get_description<F>();
  static const auto canonical = canonical_name(avnd::get_static_symbol<F>());

  using V = decltype(F::value);
  static constexpr auto range = avnd::get_range<F>();
  int flags = G_PARAM_READWRITE;
  if constexpr(std::is_same_v<int, V>)
  {
    return g_param_spec_int(
        canonical.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<unsigned int, V>)
  {
    return g_param_spec_uint(
        canonical.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<long, V>)
  {
    return g_param_spec_long(
        canonical.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<unsigned long, V>)
  {
    return g_param_spec_ulong(
        canonical.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<int64_t, V>)
  {
    return g_param_spec_int64(
        canonical.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<uint64_t, V>)
  {
    return g_param_spec_uint64(
        canonical.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<float, V>)
  {
    return g_param_spec_float(
        canonical.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<double, V>)
  {
    return g_param_spec_double(
        canonical.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(avnd::string_ish<V>)
  {
    return g_param_spec_string(
        canonical.data(), name.data(), desc.data(), std::string_view(range.init).data(),
        static_cast<GParamFlags>(flags | G_PARAM_STATIC_STRINGS));
  }
  else
  {
    return g_param_spec_string(
        canonical.data(), name.data(), desc.data(), "",
        static_cast<GParamFlags>(flags | G_PARAM_STATIC_STRINGS));
  }
}
}
