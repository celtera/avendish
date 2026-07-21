#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * A processor's persistent state.
 *
 * Declared like the other port groups:
 *
 *     struct MyProcessor
 *     {
 *       struct { ... } inputs;
 *       struct { ... } outputs;
 *       struct { std::vector<uint8_t> program; int mode{}; } state;
 *     };
 *
 * Members are saved and restored positionally through the reflection-based
 * format in state_serial.hpp, so state members -- like inlets -- are added
 * at the end. Appending, dropping trailing members and changing a member's
 * type are absorbed without any bookkeeping. Reordering or inserting in the
 * middle is detected and refused unless the processor declares a new
 * state_version together with a migration; the one edit nothing can detect
 * is swapping two members of the identical type, which is exactly what the
 * version is there to cover.
 *
 * A processor that wants to control its own bytes defines save/load on the
 * state object; those take precedence over reflection:
 *
 *     struct {
 *       std::vector<uint8_t> save() const;
 *       bool load(const uint8_t* data, std::size_t n);
 *     } state;
 *
 * Semantic changes that renaming cannot express -- a field that changes
 * meaning or unit -- are handled by declaring a version and a migration:
 *
 *     halp_meta(state_version, 3)
 *     static void migrate_state(auto& state, int from)
 *     {
 *       if(from < 2) state.cutoff_hz = normalized_to_hz(state.cutoff_hz);
 *       if(from < 3) state.mode = remap(state.mode);
 *     }
 *
 * The version is written with the state; on load the reflection pass runs
 * first, then migrate_state is called once with the version the data was
 * written at. State written by a *newer* version than the binary
 * understands is rejected rather than misinterpreted.
 */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/wrappers/state_serial.hpp>

#include <cstdint>
#include <type_traits>
#include <vector>

namespace avnd
{
// ---------------------------------------------------------------------------
// Detection

template <typename T>
concept has_state = requires(T t) { t.state; };

template <typename T>
using state_type_t = std::remove_cvref_t<decltype(std::declval<T&>().state)>;

template <typename T>
concept state_saves_itself = has_state<T> && requires(T t) {
  { t.state.save() };
};

template <typename T>
concept state_loads_itself
    = has_state<T> && requires(T t, const std::uint8_t* p, std::size_t n) {
        t.state.load(p, n);
      };

template <typename T>
concept state_is_reflectable
    = has_state<T> && avnd::state::serializable<state_type_t<T>>;

// A processor has usable state if it can either serialize itself or be
// reflected over.
template <typename T>
concept persistable
    = has_state<T> && (state_saves_itself<T> || state_is_reflectable<T>);

// ---------------------------------------------------------------------------
// Versioning

template <typename T>
consteval int state_version()
{
  if constexpr(requires { T::state_version(); })
    return T::state_version();
  else if constexpr(requires { T::state_version; })
    return T::state_version;
  else
    return 0;
}

template <typename T>
concept has_migration = requires(state_type_t<T>& s) { T::migrate_state(s, 0); };

// ---------------------------------------------------------------------------
// Save / load

template <typename T>
  requires persistable<T>
std::vector<std::uint8_t> save_object_state(const T& obj)
{
  if constexpr(state_saves_itself<T>)
  {
    const auto blob = obj.state.save();
    return std::vector<std::uint8_t>(blob.begin(), blob.end());
  }
  else
  {
    return avnd::state::save(obj.state);
  }
}

/**
 * Restores `obj.state` from `data`. `written_version` is the version the
 * blob was produced with, as recorded by the container.
 *
 * Returns false without touching the state when the data was written by a
 * newer version of the processor than this binary knows how to read.
 */
template <typename T>
  requires persistable<T>
bool load_object_state(
    T& obj, const std::uint8_t* data, std::size_t n, int written_version)
{
  constexpr int current = state_version<T>();
  if(written_version > current)
    return false;

  bool ok = false;
  if constexpr(state_loads_itself<T>)
  {
    if constexpr(requires { { obj.state.load(data, n) } -> std::same_as<bool>; })
      ok = obj.state.load(data, n);
    else
    {
      obj.state.load(data, n);
      ok = true;
    }
  }
  else
  {
    const auto res = avnd::state::load(obj.state, data, n);
    ok = res.ok;
    // A record described something other than the member at its position, so
    // the members after it cannot be trusted either -- only the processor
    // knows what moved where. Members merely appended since the data was
    // written are not this: those load cleanly and keep their defaults.
    if(ok && res.type_mismatch)
    {
      if constexpr(has_migration<T>)
      {
        if(written_version >= current)
          return false; // nothing to migrate from
      }
      else
      {
        return false;
      }
    }
  }

  if(!ok)
    return false;

  if constexpr(has_migration<T>)
  {
    if(written_version < current)
      T::migrate_state(obj.state, written_version);
  }
  return true;
}

} // namespace avnd
