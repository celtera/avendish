#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Reflection-based serialization for a processor's `state` struct.
 *
 * Every member is written as a self-describing record keyed by its *field
 * name*, not its position:
 *
 *     [u16 name length][name][u8 type tag][u32 payload size][payload]
 *
 * A reader matches records to members by name, skips records it does not
 * recognise (the size makes that possible without understanding them) and
 * leaves members that no record mentions at their default value. Adding,
 * removing and reordering members therefore needs no migration code, which
 * is the property property-store formats (LV2 state, Audio Unit ClassInfo)
 * are built around. The type tag lets a reader detect that a member changed
 * shape and fall back to its default instead of reinterpreting the bytes.
 *
 * The state struct must be a *named* type -- reflection cannot name the
 * fields of an unnamed one, and the diagnostic it produces is obscure:
 *
 *     struct state_t { int mode; };  state_t state;  // yes
 *     struct { int mode; } state;                    // no
 *
 * That is the one place this differs from the inputs/outputs groups, which
 * only ever need positional reflection. The cost is a name; the benefit is
 * that field identity survives the struct being edited.
 *
 * Supported members, recursively: trivially-copyable types, contiguous
 * containers of them (std::string, std::vector<int>, ...), aggregates, and
 * lists of any of those. Members whose type is not serializable make
 * `serializable_state` false; the processor then has to provide save/load
 * itself.
 *
 * Byte order is native: the writer and reader are the same binary. Sizes and
 * tags are fixed-width so a foreign-endian reader can at least skip records
 * rather than misparse them.
 */

#include <avnd/common/for_nth.hpp>

#include <boost/pfr.hpp>
#include <boost/pfr/core_name.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace avnd::state
{
enum class tag : std::uint8_t
{
  raw = 0,       // trivially copyable: payload is the object's bytes
  bytes = 1,     // contiguous container of trivially copyable elements
  aggregate = 2, // nested records, one per field
  list = 3,      // u32 count followed by that many encoded elements
  unknown = 255
};

// ---------------------------------------------------------------------------
// Type classification

template <typename T>
concept trivial_member = std::is_trivially_copyable_v<std::remove_cvref_t<T>>;

template <typename T>
concept contiguous_container = requires(T t) {
  { t.size() } -> std::convertible_to<std::size_t>;
  t.resize(std::size_t{});
  t.data();
} && trivial_member<std::remove_cvref_t<decltype(*std::declval<T&>().data())>>;

template <typename T>
concept list_container = requires(T t) {
  { t.size() } -> std::convertible_to<std::size_t>;
  t.clear();
  t.emplace_back();
  t.begin();
  t.end();
} && !contiguous_container<T>;

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
  if constexpr(trivial_member<type>)
    return true;
  else if constexpr(contiguous_container<type>)
    return true;
  else if constexpr(list_container<type>)
    return serializable_impl<typename type::value_type>();
  else if constexpr(std::is_aggregate_v<type>)
    return aggregate_serializable<type>(
        std::make_index_sequence<boost::pfr::tuple_size_v<type>>{});
  else
    return false;
}

template <typename T>
concept serializable = serializable_impl<std::remove_cvref_t<T>>();

template <typename T>
constexpr tag tag_of()
{
  using type = std::remove_cvref_t<T>;
  if constexpr(trivial_member<type>)
    return tag::raw;
  else if constexpr(contiguous_container<type>)
    return tag::bytes;
  else if constexpr(list_container<type>)
    return tag::list;
  else
    return tag::aggregate;
}

// ---------------------------------------------------------------------------
// Writing

struct writer
{
  std::vector<std::uint8_t> bytes;

  void raw(const void* p, std::size_t n)
  {
    const auto at = bytes.size();
    bytes.resize(at + n);
    if(n > 0)
      std::memcpy(bytes.data() + at, p, n);
  }

  template <typename T>
    requires std::is_trivially_copyable_v<T>
  void pod(const T& v)
  {
    raw(&v, sizeof(T));
  }

  // Reserve a length slot, fill it once the payload is known.
  std::size_t begin_sized()
  {
    const auto at = bytes.size();
    pod(std::uint32_t{0});
    return at;
  }
  void end_sized(std::size_t at)
  {
    const auto size = std::uint32_t(bytes.size() - at - sizeof(std::uint32_t));
    std::memcpy(bytes.data() + at, &size, sizeof(size));
  }
};

// Payload format marker, so a future change of encoding is detectable
// rather than silently misread.
inline constexpr std::uint8_t format_version = 1;

template <typename T>
void encode_value(writer& w, const T& v);

template <typename T>
void encode_fields(writer& w, const T& agg)
{
  w.pod(format_version);
  constexpr auto names = boost::pfr::names_as_array<T>();
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (
        [&] {
          const std::string_view name = names[I];
          w.pod(std::uint16_t(name.size()));
          w.raw(name.data(), name.size());
          w.pod(tag_of<boost::pfr::tuple_element_t<I, T>>());
          const auto slot = w.begin_sized();
          encode_value(w, boost::pfr::get<I>(agg));
          w.end_sized(slot);
        }(),
        ...);
  }(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
}

template <typename T>
void encode_value(writer& w, const T& v)
{
  using type = std::remove_cvref_t<T>;
  if constexpr(trivial_member<type>)
  {
    w.pod(v);
  }
  else if constexpr(contiguous_container<type>)
  {
    using elem = std::remove_cvref_t<decltype(*v.data())>;
    w.pod(std::uint32_t(v.size()));
    w.raw(v.data(), v.size() * sizeof(elem));
  }
  else if constexpr(list_container<type>)
  {
    w.pod(std::uint32_t(v.size()));
    for(const auto& e : v)
      encode_value(w, e);
  }
  else
  {
    encode_fields(w, v);
  }
}

// ---------------------------------------------------------------------------
// Reading

struct reader
{
  const std::uint8_t* data{};
  std::size_t size{};
  std::size_t pos{};
  bool ok{true};

  bool has(std::size_t n) const noexcept { return ok && pos + n <= size; }

  bool raw(void* out, std::size_t n)
  {
    if(!has(n))
      return ok = false;
    if(n > 0)
      std::memcpy(out, data + pos, n);
    pos += n;
    return true;
  }

  template <typename T>
    requires std::is_trivially_copyable_v<T>
  bool pod(T& v)
  {
    return raw(&v, sizeof(T));
  }

  void skip(std::size_t n)
  {
    if(!has(n))
      ok = false;
    else
      pos += n;
  }
};

template <typename T>
bool decode_value(reader& r, T& v, std::size_t payload_size);

template <typename T>
bool decode_fields(reader& r, T& agg, std::size_t payload_size)
{
  const std::size_t end = r.pos + payload_size;

  std::uint8_t fmt{};
  if(!r.pod(fmt))
    return false;
  if(fmt != format_version)
    return false;

  while(r.ok && r.pos < end)
  {
    std::uint16_t name_len{};
    if(!r.pod(name_len))
      return false;
    if(!r.has(name_len))
      return false;
    const std::string_view name{
        reinterpret_cast<const char*>(r.data + r.pos), name_len};
    r.pos += name_len;

    tag t{};
    std::uint32_t size{};
    if(!r.pod(t) || !r.pod(size))
      return false;
    if(!r.has(size))
      return false;

    const std::size_t payload_start = r.pos;
    bool matched = false;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (
          [&] {
            constexpr auto names = boost::pfr::names_as_array<T>();
            if(matched || names[I] != name)
              return;
            using field_t = boost::pfr::tuple_element_t<I, T>;
            matched = true;
            // A member that changed shape keeps its default rather than
            // reinterpreting bytes written for a different type.
            if(t != tag_of<field_t>())
              return;
            decode_value(r, boost::pfr::get<I>(agg), size);
          }(),
          ...);
    }(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});

    // Unknown, mismatched or partially-read records: resume at the next one.
    r.pos = payload_start;
    r.skip(size);
  }
  return r.ok;
}

template <typename T>
bool decode_value(reader& r, T& v, std::size_t payload_size)
{
  using type = std::remove_cvref_t<T>;
  if constexpr(trivial_member<type>)
  {
    return r.pod(v);
  }
  else if constexpr(contiguous_container<type>)
  {
    using elem = std::remove_cvref_t<decltype(*v.data())>;
    std::uint32_t count{};
    if(!r.pod(count))
      return false;
    // Trust the record size, not the count: a corrupt count must not make us
    // allocate or read out of bounds.
    if(count > (payload_size - sizeof(count)) / sizeof(elem))
      return false;
    v.resize(count);
    return r.raw(v.data(), std::size_t(count) * sizeof(elem));
  }
  else if constexpr(list_container<type>)
  {
    std::uint32_t count{};
    if(!r.pod(count))
      return false;
    v.clear();
    const std::size_t end = r.pos + (payload_size - sizeof(count));
    for(std::uint32_t i = 0; i < count && r.ok && r.pos < end; i++)
    {
      v.emplace_back();
      if(!decode_value(r, *std::prev(v.end()), end - r.pos))
        return false;
    }
    return r.ok;
  }
  else
  {
    return decode_fields(r, v, payload_size);
  }
}

// ---------------------------------------------------------------------------
// Entry points

template <typename S>
  requires serializable<S>
std::vector<std::uint8_t> save(const S& s)
{
  writer w;
  encode_fields(w, s);
  return std::move(w.bytes);
}

// Members not mentioned by the blob keep the value they already hold, so the
// caller decides what "missing" means by pre-seeding defaults. An empty
// payload mentions nothing and therefore succeeds without changing anything;
// deciding whether a host handing over *no state at all* is an error belongs
// to the container above this one.
template <typename S>
  requires serializable<S>
bool load(S& s, const std::uint8_t* data, std::size_t n)
{
  if(n == 0)
    return true;
  if(!data)
    return false;
  reader r{data, n, 0, true};
  return decode_fields(r, s, n);
}

} // namespace avnd::state
