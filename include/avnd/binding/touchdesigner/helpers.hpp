#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <cstring>
#include <string>
#include <string_view>

namespace touchdesigner
{

/**
 * Get a valid TouchDesigner parameter name from an Avendish field
 * TD requires: starts with uppercase, no spaces, only a-zA-Z0-9
 */
template <typename Field>
consteval auto get_td_name()
{
  constexpr auto name = avnd::get_name<Field>();

  // For now, just return as-is
  // TODO: sanitize if needed (remove spaces, ensure starts with uppercase)
  return name;
}

/**
 * Get a valid TouchDesigner symbol name (C-name style)
 * Used for opType, channel names, etc.
 */
template <typename T>
consteval auto get_c_name()
{
  if constexpr (requires { T::c_name(); })
  {
    return T::c_name();
  }
  else if constexpr (requires { T::name(); })
  {
    return T::name();
  }
  else
  {
    return "untitled";
  }
}

/**
 * Extract display name for TouchDesigner UI
 */
template <typename T>
consteval auto get_display_name()
{
  if constexpr (requires { T::name(); })
  {
    return T::name();
  }
  else if constexpr (requires { T::c_name(); })
  {
    return T::c_name();
  }
  else
  {
    return "Untitled";
  }
}

/**
 * Get UUID for the processor
 */
template <typename T>
consteval auto get_uuid()
{
  if constexpr (requires { T::uuid(); })
  {
    return T::uuid();
  }
  else
  {
    return "";
  }
}

/**
 * Get author information
 */
template <typename T>
consteval auto get_author()
{
  if constexpr (requires { T::author(); })
  {
    return T::author();
  }
  else
  {
    return "Unknown";
  }
}

/**
 * Detect number of input channels for audio processors
 */
template <typename T>
constexpr int input_channels_count()
{
  if constexpr (avnd::audio_processor<T>)
  {
    return avnd::input_channels<T>(1);
  }
  else
  {
    return 0;
  }
}

/**
 * Detect number of output channels for audio processors
 */
template <typename T>
constexpr int output_channels_count()
{
  if constexpr (avnd::audio_processor<T>)
  {
    return avnd::output_channels<T>(1);
  }
  else
  {
    return 0;
  }
}

/**
 * Check if processor has textures (for TOP detection)
 */
template <typename T>
constexpr bool has_texture_ports()
{
  // Will implement texture detection when we add TOP support
  return false;
}

/**
 * Check if processor is an audio processor (for CHOP)
 */
template <typename T>
constexpr bool is_audio_chop()
{
  return avnd::audio_processor<T>;
}

/**
 * Runtime name conversion utilities
 */
inline std::string sanitize_td_name(std::string_view name)
{
  std::string result;
  result.reserve(name.size());

  bool first = true;
  for (char c : name)
  {
    if (first)
    {
      // First character must be uppercase letter
      if (c >= 'a' && c <= 'z')
        result += (c - 'a' + 'A');
      else if ((c >= 'A' && c <= 'Z'))
        result += c;
      else
        continue; // Skip invalid first character
      first = false;
    }
    else
    {
      // Rest can be alphanumeric
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        result += c;
      // Skip spaces and special characters
    }
  }

  if (result.empty())
    return "Param";

  return result;
}

} // namespace touchdesigner
