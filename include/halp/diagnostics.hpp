#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/diagnostics.hpp>

#include <halp/modules.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>

#if __has_include(<fmt/format.h>) && !defined(AVND_DISABLE_FMT)
#include <fmt/format.h>
#define HALP_DIAGNOSTICS_FMT 1
#endif

HALP_MODULE_EXPORT
namespace halp
{

/**
 * A condition an object reports about itself, for the host to display.
 *
 *   struct MyObject
 *   {
 *     halp::diagnostics diagnostics;
 *
 *     void operator()()
 *     {
 *       if(!m_connected)
 *         diagnostics.warning("no sender connected on port {}", port);
 *       diagnostics.info("{} points", count);
 *     }
 *   };
 *
 * State what is true on each run; the binding clears the set before calling the
 * object, so a condition disappears simply by not being restated.
 *
 * An optional compile-time identifier can be attached, for hosts that suppress
 * or localise by id, for structured codes (GStreamer domains), and for tests
 * that would rather assert on a condition than on English:
 *
 *   diagnostics.error<"port_in_use">("port {} is already in use", port);
 *   REQUIRE(obj.diagnostics.has<"port_in_use">());
 *
 * The id has to be a template argument rather than a leading string: passing it
 * as one would be ambiguous with the format string. It is compile-time because
 * a stable id is by definition not computed at runtime, so only a pointer to a
 * literal is ever stored.
 *
 * Fixed capacity throughout, so raising a diagnostic never allocates and is
 * usable from an audio callback. Text beyond the capacity is truncated;
 * entries beyond MaxEntries are dropped, lowest severity first, and counted.
 */
template <std::size_t MaxEntries = 8, std::size_t Capacity = 128>
struct basic_diagnostics
{
  struct entry
  {
    avnd::diagnostic_severity severity{};
    std::string_view id{};
    std::array<char, Capacity> text{};
    uint16_t length{};

    std::string_view message() const noexcept { return {text.data(), length}; }
  };

  std::array<entry, MaxEntries> entries{};
  uint8_t count{};
  uint8_t dropped{};

  void clear() noexcept
  {
    count = 0;
    dropped = 0;
  }

  bool empty() const noexcept { return count == 0; }

  avnd::diagnostic_severity worst() const noexcept
  {
    auto s = avnd::diagnostic_severity::info;
    for(uint8_t i = 0; i < count; ++i)
      if(entries[i].severity > s)
        s = entries[i].severity;
    return s;
  }

  bool has(std::string_view id) const noexcept
  {
    for(uint8_t i = 0; i < count; ++i)
      if(entries[i].id == id)
        return true;
    return false;
  }

  template <static_string Id>
  bool has() const noexcept
  {
    return has(std::string_view{Id.value});
  }

  // ---- raising -----------------------------------------------------------
#if defined(HALP_DIAGNOSTICS_FMT)
  template <typename... A>
  void raise(
      avnd::diagnostic_severity s, std::string_view id, fmt::format_string<A...> f,
      A&&... args)
  {
    if(entry* e = acquire(s))
    {
      e->id = id;
      auto res = fmt::format_to_n(
          e->text.data(), Capacity, f, std::forward<A>(args)...);
      e->length = static_cast<uint16_t>(res.size < Capacity ? res.size : Capacity);
    }
  }

#define HALP_DIAG_LEVEL(name, sev)                                            \
  template <typename... A>                                                    \
  void name(fmt::format_string<A...> f, A&&... args)                          \
  {                                                                           \
    raise(sev, {}, f, std::forward<A>(args)...);                              \
  }                                                                           \
  template <static_string Id, typename... A>                                  \
  void name(fmt::format_string<A...> f, A&&... args)                          \
  {                                                                           \
    raise(sev, std::string_view{Id.value}, f, std::forward<A>(args)...);      \
  }
#else
  // No fmt: accept a plain message, still fixed-capacity.
  void raise(avnd::diagnostic_severity s, std::string_view id, std::string_view msg)
  {
    if(entry* e = acquire(s))
    {
      e->id = id;
      const std::size_t n = msg.size() < Capacity ? msg.size() : Capacity;
      std::memcpy(e->text.data(), msg.data(), n);
      e->length = static_cast<uint16_t>(n);
    }
  }

#define HALP_DIAG_LEVEL(name, sev)                                            \
  void name(std::string_view msg) { raise(sev, {}, msg); }                    \
  template <static_string Id>                                                 \
  void name(std::string_view msg)                                             \
  {                                                                           \
    raise(sev, std::string_view{Id.value}, msg);                              \
  }
#endif

  HALP_DIAG_LEVEL(info, avnd::diagnostic_severity::info)
  HALP_DIAG_LEVEL(warning, avnd::diagnostic_severity::warning)
  HALP_DIAG_LEVEL(error, avnd::diagnostic_severity::error)
  HALP_DIAG_LEVEL(fatal, avnd::diagnostic_severity::fatal)
#undef HALP_DIAG_LEVEL

private:
  // Returns the slot to write into, or nullptr if this diagnostic is dropped.
  // When full, the lowest-severity entry is evicted so the most serious
  // condition is never the one lost.
  entry* acquire(avnd::diagnostic_severity s) noexcept
  {
    if(count < MaxEntries)
    {
      entry* e = &entries[count++];
      e->severity = s;
      e->length = 0;
      e->id = {};
      return e;
    }

    uint8_t weakest = 0;
    for(uint8_t i = 1; i < count; ++i)
      if(entries[i].severity < entries[weakest].severity)
        weakest = i;

    ++dropped;
    if(entries[weakest].severity >= s)
      return nullptr;

    entry* e = &entries[weakest];
    e->severity = s;
    e->length = 0;
    e->id = {};
    return e;
  }
};

using diagnostics = basic_diagnostics<>;

}
