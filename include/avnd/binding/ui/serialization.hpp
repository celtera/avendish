#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Minimal binary serialization for UI <-> processor bus payloads that must
 * cross a host boundary (VST3 IMessage). Not a general-purpose format:
 * both ends are the same binary, so native layout/endianness is fine.
 *
 * Supported message shapes, recursively:
 *  - trivially-copyable types: raw bytes
 *  - std::string / std::vector<trivially-copyable>: u32 length + bytes
 *  - aggregates of the above (via boost.pfr)
 * Anything else makes `serializable` false and the caller keeps its
 * previous behaviour (bus disabled for that direction).
 */

#include <boost/pfr.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace avnd::bus_serial
{
template <typename T>
concept trivial_payload = std::is_trivially_copyable_v<T>;

template <typename T>
concept sized_payload = requires(T t) {
  { t.size() } -> std::convertible_to<std::size_t>;
  t.resize(std::size_t{});
  t.data();
} && trivial_payload<std::remove_cvref_t<decltype(*std::declval<T>().data())>>;

template <typename T>
constexpr bool serializable_impl();

template <typename T, std::size_t... I>
constexpr bool aggregate_serializable(std::index_sequence<I...>)
{
  return (serializable_impl<boost::pfr::tuple_element_t<I, T>>() && ...);
}

template <typename T>
constexpr bool serializable_impl()
{
  using type = std::remove_cvref_t<T>;
  if constexpr(trivial_payload<type>)
    return true;
  else if constexpr(sized_payload<type>)
    return true;
  else if constexpr(std::is_aggregate_v<type>)
    return aggregate_serializable<type>(
        std::make_index_sequence<boost::pfr::tuple_size_v<type>>{});
  else
    return false;
}

template <typename T>
concept serializable = !std::is_void_v<T> && serializable_impl<T>();

// ---- size ----
template <typename T>
inline std::size_t byte_size(const T& v)
{
  using type = std::remove_cvref_t<T>;
  if constexpr(trivial_payload<type>)
  {
    return sizeof(type);
  }
  else if constexpr(sized_payload<type>)
  {
    using elt = std::remove_cvref_t<decltype(*v.data())>;
    return sizeof(uint32_t) + v.size() * sizeof(elt);
  }
  else
  {
    std::size_t n = 0;
    boost::pfr::for_each_field(v, [&](const auto& f) { n += byte_size(f); });
    return n;
  }
}

// ---- write ----
template <typename T>
inline void write(std::vector<char>& out, const T& v)
{
  using type = std::remove_cvref_t<T>;
  if constexpr(trivial_payload<type>)
  {
    const auto pos = out.size();
    out.resize(pos + sizeof(type));
    std::memcpy(out.data() + pos, &v, sizeof(type));
  }
  else if constexpr(sized_payload<type>)
  {
    using elt = std::remove_cvref_t<decltype(*v.data())>;
    const uint32_t count = (uint32_t)v.size();
    const auto pos = out.size();
    out.resize(pos + sizeof(uint32_t) + count * sizeof(elt));
    std::memcpy(out.data() + pos, &count, sizeof(uint32_t));
    if(count > 0)
      std::memcpy(out.data() + pos + sizeof(uint32_t), v.data(), count * sizeof(elt));
  }
  else
  {
    boost::pfr::for_each_field(v, [&](const auto& f) { write(out, f); });
  }
}

template <typename T>
inline std::vector<char> to_bytes(const T& v)
{
  std::vector<char> out;
  out.reserve(byte_size(v));
  write(out, v);
  return out;
}

// ---- read ----
// Returns false on truncated / malformed input (bounds-checked: bus
// payloads arrive from the host and must not be trusted blindly).
template <typename T>
inline bool read(const char* data, std::size_t size, std::size_t& pos, T& v)
{
  using type = std::remove_cvref_t<T>;
  if constexpr(trivial_payload<type>)
  {
    if(pos + sizeof(type) > size)
      return false;
    std::memcpy(&v, data + pos, sizeof(type));
    pos += sizeof(type);
    return true;
  }
  else if constexpr(sized_payload<type>)
  {
    using elt = std::remove_cvref_t<decltype(*v.data())>;
    uint32_t count{};
    if(pos + sizeof(uint32_t) > size)
      return false;
    std::memcpy(&count, data + pos, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    if(count > (size - pos) / sizeof(elt))
      return false;
    v.resize(count);
    if(count > 0)
      std::memcpy(v.data(), data + pos, count * sizeof(elt));
    pos += count * sizeof(elt);
    return true;
  }
  else
  {
    bool ok = true;
    boost::pfr::for_each_field(v, [&](auto& f) { ok = ok && read(data, size, pos, f); });
    return ok;
  }
}

template <typename T>
inline bool from_bytes(const void* data, std::size_t size, T& v)
{
  std::size_t pos = 0;
  return read((const char*)data, size, pos, v) && pos == size;
}
}
