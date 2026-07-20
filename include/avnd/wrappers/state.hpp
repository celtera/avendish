#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/concepts/state.hpp>

#include <cstring>

namespace avnd
{
/**
 * Normalized access to the custom-state spellings (see concepts/state.hpp).
 * Bindings call these; the accepted user spellings never leak further.
 */

// Stream the object's custom state into sink(const void*, std::size_t).
// The sink may be called multiple times; bytes are concatenated.
template <typename T, typename F>
  requires has_save_state<T> && state_writer<F>
void save_state(T& obj, F&& sink)
{
  if constexpr(has_save_state_writer<T>)
  {
    obj.save_state(sink);
  }
  else
  {
    static_assert(has_save_state_container<T>);
    auto&& bytes = obj.save_state();
    if(bytes.size() > 0)
      sink(bytes.data(), bytes.size());
  }
}

// Load a blob into the object. Returns false if the object rejected it
// (void-returning load spellings cannot fail and thus always succeed).
template <has_load_state T>
bool load_state(T& obj, const std::uint8_t* data, std::size_t n)
{
  const auto call = [&](auto&&... args) -> bool {
    if constexpr(requires {
                   { obj.load_state(args...) } -> std::convertible_to<bool>;
                 })
      return static_cast<bool>(obj.load_state(args...));
    else
    {
      obj.load_state(args...);
      return true;
    }
  };

  if constexpr(requires { obj.load_state(data, n); })
    return call(data, n);
  else if constexpr(requires { obj.load_state((const char*)data, n); })
    return call((const char*)data, n);
  else if constexpr(requires { obj.load_state((const std::byte*)data, n); })
    return call((const std::byte*)data, n);
  else
  {
    static_assert(has_load_state_span<T>);
    if constexpr(requires {
                   { obj.load_state({data, n}) } -> std::convertible_to<bool>;
                 })
      return static_cast<bool>(obj.load_state({data, n}));
    else
    {
      obj.load_state({data, n});
      return true;
    }
  }
}

}
