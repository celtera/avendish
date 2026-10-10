#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Reflection-based serialization for a processor's `state` struct.
 *
 * Members are written in declaration order, each as a self-describing
 * record:
 *
 *     [u8 type tag][u8 scalar kind][u32 payload size][payload]
 *
 * preceded by a header holding the format marker, a hash of the struct's
 * layout and the number of records. Positional encoding means the state
 * struct can be an unnamed type, exactly like the inputs and outputs groups.
 *
 * What the shape of the data buys, without any version bookkeeping:
 *
 *  - appending a member: older data simply has fewer records, and the new
 *    member keeps its default;
 *  - dropping trailing members: the extra records are skipped;
 *  - a member whose type changed: the record no longer describes the member
 *    at that position -- including an int becoming a float, which the
 *    scalar kind distinguishes despite the identical width -- so it is
 *    skipped instead of being reinterpreted.
 *
 * A record that does not describe the member at its position is reported as
 * a type mismatch, which tells the caller the remaining positions cannot be
 * trusted; it refuses the load unless the processor declared a migration.
 * Appending members and dropping trailing ones are not mismatches.
 *
 * The one edit nothing here can see is a swap of two members of the
 * identical type, or a change in what a member *means*: that is what
 * state_version and migrate_state are for.
 *
 * Member types are classified with the same concepts the value bindings
 * use (avnd/concepts/generic.hpp), so anything a processor may carry is
 * covered: scalars and enums, strings, vectors and other list containers,
 * sets and maps, optionals, variants, pairs and tuples, fixed arrays,
 * aggregates, and any nesting of those.
 *
 * What is refused, at compile time: anything that only *borrows* its data.
 * A pointer, reference or view is trivially copyable, so it would be
 * written out as an address that means nothing to the process reading it
 * back. A processor needing one provides save/load on its state instead.
 *
 * Byte order is native: writer and reader are the same binary. Sizes and
 * tags are fixed-width so a foreign-endian reader can at least skip records
 * rather than misparse them.
 */

#include <avnd/concepts/generic.hpp>

#include <avnd/common/aggregates.hpp>
#include <avnd/common/for_nth.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace avnd::state
{
enum class tag : std::uint8_t
{
  raw = 0,        // trivially copyable: payload is the object's bytes
  bytes = 1,      // contiguous block of trivially copyable elements
  aggregate = 2,  // nested records, one per field
  list = 3,       // u32 count, then that many encoded elements
  optional = 4,   // u8 engaged, then the value if engaged
  variant = 5,    // u32 alternative index, then that alternative
  map = 6,        // u32 count, then key/value pairs
  set = 7,        // u32 count, then keys
  tuple = 8,      // the elements in order, no count
  fixed_array = 9 // u32 count, then that many elements
};

// ---------------------------------------------------------------------------
// Classification.
//
// Order matters: a std::string is span_ish and list_ish, a std::array is
// span_ish and tuple_ish, so the more specific test comes first.

template <typename T>
concept trivial_member = std::is_trivially_copyable_v<std::remove_cvref_t<T>>;

template <typename T>
concept byte_block = avnd::vector_ish<T> && requires(T t) {
  t.data();
} && trivial_member<typename std::remove_cvref_t<T>::value_type>;

// A string we own. std::string_view is string_ish too, but it borrows its
// characters, so it must not take this path.
template <typename T>
concept string_member = avnd::string_ish<T> && requires(T& t) {
  t.resize(std::size_t{});
  { t.data() } -> std::convertible_to<const char*>;
};

// A type that only borrows: data() is a pointer and the size is not ours to
// change, so the bytes we would store are an address.
template <typename T>
concept view_like = requires(const T& t) {
  t.data();
  { t.size() } -> std::convertible_to<std::size_t>;
} && std::is_pointer_v<decltype(std::declval<const T&>().data())>
     && !requires(T& t) { t.resize(std::size_t{}); }
     && !avnd::tuple_ish<std::remove_cvref_t<T>>;

template <typename T>
constexpr bool borrows_memory();

template <typename T, std::size_t... I>
constexpr bool any_member_borrows(std::index_sequence<I...>)
{
  return (borrows_memory<avnd::pfr::tuple_element_t<I, T>>() || ...);
}

template <typename T, std::size_t... I>
constexpr bool any_element_borrows(std::index_sequence<I...>)
{
  return (borrows_memory<std::tuple_element_t<I, T>>() || ...);
}

template <typename T, std::size_t... I>
constexpr bool any_alternative_borrows(std::index_sequence<I...>)
{
  return (borrows_memory<std::variant_alternative_t<I, T>>() || ...);
}

template <typename T>
constexpr bool borrows_memory()
{
  using type = std::remove_cvref_t<T>;
  if constexpr(std::is_pointer_v<type> || std::is_reference_v<type>)
    return true;
  else if constexpr(std::is_array_v<type>)
    return borrows_memory<std::remove_extent_t<type>>();
  else if constexpr(string_member<type>)
    return false;
  else if constexpr(avnd::optional_ish<type>)
    return borrows_memory<typename type::value_type>();
  else if constexpr(avnd::variant_ish<type>)
    return any_alternative_borrows<type>(
        std::make_index_sequence<std::variant_size_v<type>>{});
  else if constexpr(avnd::map_ish<type>)
    return borrows_memory<typename type::key_type>()
           || borrows_memory<typename type::mapped_type>();
  else if constexpr(avnd::set_ish<type>)
    return borrows_memory<typename type::key_type>();
  else if constexpr(avnd::vector_ish<type>)
    return borrows_memory<typename type::value_type>();
  else if constexpr(avnd::tuple_ish<type>)
    return any_element_borrows<type>(
        std::make_index_sequence<std::tuple_size_v<type>>{});
  else if constexpr(view_like<type>)
    return true;
  else if constexpr(std::is_aggregate_v<type>)
    return any_member_borrows<type>(
        std::make_index_sequence<avnd::pfr::tuple_size_v<type>>{});
  else
    return false;
}

template <typename T>
constexpr bool serializable_impl();

template <typename T, std::size_t... I>
constexpr bool aggregate_serializable(std::index_sequence<I...>)
{
  return (serializable_impl<avnd::pfr::tuple_element_t<I, T>>() && ...);
}

template <typename T, std::size_t... I>
constexpr bool elements_serializable(std::index_sequence<I...>)
{
  return (serializable_impl<std::tuple_element_t<I, T>>() && ...);
}

template <typename T, std::size_t... I>
constexpr bool alternatives_serializable(std::index_sequence<I...>)
{
  return (serializable_impl<std::variant_alternative_t<I, T>>() && ...);
}

template <typename T>
constexpr bool serializable_impl()
{
  using type = std::remove_cvref_t<T>;
  if constexpr(borrows_memory<type>())
    return false;
  else if constexpr(string_member<type>)
    return true;
  else if constexpr(avnd::optional_ish<type>)
    return serializable_impl<typename type::value_type>();
  else if constexpr(avnd::variant_ish<type>)
    return alternatives_serializable<type>(
        std::make_index_sequence<std::variant_size_v<type>>{});
  else if constexpr(avnd::map_ish<type>)
    return serializable_impl<typename type::key_type>()
           && serializable_impl<typename type::mapped_type>();
  else if constexpr(avnd::set_ish<type>)
    return serializable_impl<typename type::key_type>();
  else if constexpr(byte_block<type>)
    return true;
  else if constexpr(avnd::vector_ish<type>)
    return serializable_impl<typename type::value_type>();
  else if constexpr(avnd::tuple_ish<type>)
    return elements_serializable<type>(
        std::make_index_sequence<std::tuple_size_v<type>>{});
  else if constexpr(trivial_member<type>)
    return true;
  else if constexpr(std::is_aggregate_v<type>)
    return aggregate_serializable<type>(
        std::make_index_sequence<avnd::pfr::tuple_size_v<type>>{});
  else
    return false;
}

template <typename T>
concept serializable = serializable_impl<std::remove_cvref_t<T>>();

template <typename T>
constexpr tag tag_of()
{
  using type = std::remove_cvref_t<T>;
  if constexpr(string_member<type>)
    return tag::bytes;
  else if constexpr(avnd::optional_ish<type>)
    return tag::optional;
  else if constexpr(avnd::variant_ish<type>)
    return tag::variant;
  else if constexpr(avnd::map_ish<type>)
    return tag::map;
  else if constexpr(avnd::set_ish<type>)
    return tag::set;
  else if constexpr(byte_block<type>)
    return tag::bytes;
  else if constexpr(avnd::vector_ish<type>)
    return tag::list;
  else if constexpr(avnd::tuple_ish<type>)
    return std::is_trivially_copyable_v<type> ? tag::raw : tag::tuple;
  else if constexpr(trivial_member<type>)
    return tag::raw;
  else
    return tag::aggregate;
}

// ---------------------------------------------------------------------------
// Scalar kind: distinguishes types of equal width (an int from a float) so
// one is never read back as the other.

enum class scalar_kind : std::uint32_t
{
  none = 0,
  boolean = 1,
  signed_int = 2,
  unsigned_int = 3,
  floating = 4,
  enumeration = 5,
  composite = 6
};

template <typename T>
constexpr scalar_kind kind_of()
{
  using type = std::remove_cvref_t<T>;
  if constexpr(std::is_same_v<type, bool>)
    return scalar_kind::boolean;
  else if constexpr(std::is_enum_v<type>)
    return scalar_kind::enumeration;
  else if constexpr(std::is_floating_point_v<type>)
    return scalar_kind::floating;
  else if constexpr(std::is_integral_v<type>)
    return std::is_signed_v<type> ? scalar_kind::signed_int
                                  : scalar_kind::unsigned_int;
  else if constexpr(trivial_member<type>)
    return scalar_kind::composite;
  else
    return scalar_kind::none;
}

// ---------------------------------------------------------------------------
// Layout hash

constexpr std::uint32_t hash_step(std::uint32_t h, std::uint32_t v)
{
  h ^= v;
  return h * 16777619u; // FNV-1a
}

template <typename T>
constexpr std::uint32_t layout_hash_of(std::uint32_t h = 2166136261u);

template <typename T, std::size_t... I>
constexpr std::uint32_t
layout_hash_fields(std::uint32_t h, std::index_sequence<I...>)
{
  ((h = layout_hash_of<avnd::pfr::tuple_element_t<I, T>>(h)), ...);
  return h;
}

template <typename T, std::size_t... I>
constexpr std::uint32_t
layout_hash_elements(std::uint32_t h, std::index_sequence<I...>)
{
  ((h = layout_hash_of<std::tuple_element_t<I, T>>(h)), ...);
  return h;
}

template <typename T, std::size_t... I>
constexpr std::uint32_t
layout_hash_alternatives(std::uint32_t h, std::index_sequence<I...>)
{
  ((h = layout_hash_of<std::variant_alternative_t<I, T>>(h)), ...);
  return h;
}

template <typename T>
constexpr std::uint32_t layout_hash_of(std::uint32_t h)
{
  using type = std::remove_cvref_t<T>;
  h = hash_step(h, std::uint32_t(tag_of<type>()));
  h = hash_step(h, std::uint32_t(kind_of<type>()));

  if constexpr(string_member<type>)
    return h;
  else if constexpr(avnd::optional_ish<type>)
    return layout_hash_of<typename type::value_type>(h);
  else if constexpr(avnd::variant_ish<type>)
    return layout_hash_alternatives<type>(
        hash_step(h, std::uint32_t(std::variant_size_v<type>)),
        std::make_index_sequence<std::variant_size_v<type>>{});
  else if constexpr(avnd::map_ish<type>)
    return layout_hash_of<typename type::mapped_type>(
        layout_hash_of<typename type::key_type>(h));
  else if constexpr(avnd::set_ish<type>)
    return layout_hash_of<typename type::key_type>(h);
  else if constexpr(byte_block<type>)
    return layout_hash_of<typename type::value_type>(h);
  else if constexpr(avnd::vector_ish<type>)
    return layout_hash_of<typename type::value_type>(h);
  else if constexpr(avnd::tuple_ish<type>)
    return layout_hash_elements<type>(
        hash_step(h, std::uint32_t(std::tuple_size_v<type>)),
        std::make_index_sequence<std::tuple_size_v<type>>{});
  // A trivially-copyable aggregate is one blob on the wire, but its field
  // shapes still feed the hash so that {int, float} and {float, int} differ.
  else if constexpr(std::is_aggregate_v<type> && !std::is_array_v<type>)
    return layout_hash_fields<type>(
        hash_step(h, std::uint32_t(avnd::pfr::tuple_size_v<type>)),
        std::make_index_sequence<avnd::pfr::tuple_size_v<type>>{});
  else if constexpr(trivial_member<type>)
    return hash_step(h, std::uint32_t(sizeof(type)));
  else
    return layout_hash_fields<type>(
        hash_step(h, std::uint32_t(avnd::pfr::tuple_size_v<type>)),
        std::make_index_sequence<avnd::pfr::tuple_size_v<type>>{});
}

// ---------------------------------------------------------------------------
// Writing

inline constexpr std::uint8_t format_version = 1;

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

template <typename T>
void encode_value(writer& w, const T& v);

// One record: what it is, then how long it is, then it.
template <typename T>
void encode_record(writer& w, const T& v)
{
  w.pod(tag_of<T>());
  w.pod(std::uint8_t(kind_of<T>()));
  const auto slot = w.begin_sized();
  encode_value(w, v);
  w.end_sized(slot);
}

template <typename T>
void encode_fields(writer& w, const T& agg)
{
  w.pod(format_version);
  w.pod(layout_hash_of<T>());
  w.pod(std::uint16_t(avnd::pfr::tuple_size_v<T>));

  avnd::for_each_field_ref(agg, [&](const auto& field) {
    encode_record(w, field);
  });
}

template <typename T>
void encode_value(writer& w, const T& v)
{
  using type = std::remove_cvref_t<T>;
  if constexpr(string_member<type>)
  {
    w.pod(std::uint32_t(v.size()));
    w.raw(v.data(), v.size());
  }
  else if constexpr(avnd::optional_ish<type>)
  {
    w.pod(std::uint8_t(bool(v) ? 1 : 0));
    if(bool(v))
      encode_value(w, *v);
  }
  else if constexpr(avnd::variant_ish<type>)
  {
    w.pod(std::uint32_t(v.index()));
    std::visit([&](const auto& alt) { encode_value(w, alt); }, v);
  }
  else if constexpr(avnd::map_ish<type>)
  {
    w.pod(std::uint32_t(v.size()));
    for(const auto& [key, mapped] : v)
    {
      encode_value(w, key);
      encode_value(w, mapped);
    }
  }
  else if constexpr(avnd::set_ish<type>)
  {
    w.pod(std::uint32_t(v.size()));
    for(const auto& key : v)
      encode_value(w, key);
  }
  else if constexpr(byte_block<type>)
  {
    using elem = typename type::value_type;
    w.pod(std::uint32_t(v.size()));
    w.raw(v.data(), v.size() * sizeof(elem));
  }
  else if constexpr(avnd::vector_ish<type>)
  {
    w.pod(std::uint32_t(v.size()));
    for(const auto& e : v)
      encode_value(w, e);
  }
  else if constexpr(avnd::tuple_ish<type> && !std::is_trivially_copyable_v<type>)
  {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (encode_value(w, std::get<I>(v)), ...);
    }(std::make_index_sequence<std::tuple_size_v<type>>{});
  }
  else if constexpr(trivial_member<type>)
  {
    w.pod(v);
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

struct load_result
{
  bool ok{false};

  // A record did not describe the member sitting at its position. Members
  // appended since, and records left over from members since removed, do
  // *not* set this: only a genuine disagreement about what lives where,
  // which means the remaining positions can no longer be trusted.
  bool type_mismatch{false};

  // The layout hash differed. True for any structural edit, including the
  // benign ones, so it is informational rather than a reason to refuse.
  bool layout_changed{false};

  explicit operator bool() const noexcept { return ok; }
};

template <typename T>
bool decode_value(reader& r, T& v, std::size_t payload_size);

// Construct variant alternative `index` and decode into it.
template <typename V, std::size_t I = 0>
bool decode_variant(reader& r, V& v, std::uint32_t index, std::size_t payload)
{
  if constexpr(I < std::variant_size_v<V>)
  {
    if(index == I)
    {
      std::variant_alternative_t<I, V> alt{};
      if(!decode_value(r, alt, payload))
        return false;
      v = std::move(alt);
      return true;
    }
    return decode_variant<V, I + 1>(r, v, index, payload);
  }
  else
  {
    return false; // an alternative this build does not have
  }
}

template <typename T>
bool decode_fields(
    reader& r, T& agg, std::size_t payload_size, bool& mismatch, bool& changed)
{
  const std::size_t end = r.pos + payload_size;

  std::uint8_t fmt{};
  std::uint32_t hash{};
  std::uint16_t count{};
  if(!r.pod(fmt) || !r.pod(hash) || !r.pod(count))
    return false;
  if(fmt != format_version)
    return false;
  if(hash != layout_hash_of<T>())
    changed = true;

  constexpr std::size_t fields = avnd::pfr::tuple_size_v<T>;
  if(count != fields)
    changed = true;

  // One record per field, in declaration order. A record whose shape does
  // not match its field is skipped (mismatch); a field with no record left
  // keeps its default (appended member).
  std::size_t index = 0;
  avnd::for_each_field_ref(agg, [&](auto& field) {
    if(!r.ok || index >= count)
    {
      index++;
      return;
    }
    tag t{};
    std::uint8_t k{};
    std::uint32_t size{};
    if(!(r.pod(t) && r.pod(k) && r.pod(size) && r.has(size)))
    {
      r.ok = false;
      return;
    }
    const std::size_t payload_start = r.pos;
    using field_t = std::remove_cvref_t<decltype(field)>;
    if(t != tag_of<field_t>() || k != std::uint8_t(kind_of<field_t>()))
      mismatch = true;
    else
      decode_value(r, field, size);
    r.pos = payload_start;
    r.skip(size);
    index++;
  });

  // Records past the last field (members removed since) are skipped.
  while(r.ok && index < count && r.pos < end)
  {
    tag t{};
    std::uint8_t k{};
    std::uint32_t size{};
    if(!(r.pod(t) && r.pod(k) && r.pod(size) && r.has(size)))
      return false;
    r.skip(size);
    index++;
  }
  return r.ok;
}

template <typename T>
bool decode_value(reader& r, T& v, std::size_t payload_size)
{
  using type = std::remove_cvref_t<T>;
  const std::size_t end = r.pos + payload_size;

  if constexpr(string_member<type>)
  {
    std::uint32_t count{};
    if(!r.pod(count))
      return false;
    if(count > payload_size - sizeof(count))
      return false;
    v.resize(count);
    return count == 0 ? true : r.raw(v.data(), count);
  }
  else if constexpr(avnd::optional_ish<type>)
  {
    std::uint8_t engaged{};
    if(!r.pod(engaged))
      return false;
    if(!engaged)
    {
      v.reset();
      return true;
    }
    typename type::value_type inner{};
    if(!decode_value(r, inner, end - r.pos))
      return false;
    v = std::move(inner);
    return true;
  }
  else if constexpr(avnd::variant_ish<type>)
  {
    std::uint32_t index{};
    if(!r.pod(index))
      return false;
    return decode_variant(r, v, index, end - r.pos);
  }
  else if constexpr(avnd::map_ish<type>)
  {
    std::uint32_t count{};
    if(!r.pod(count))
      return false;
    v.clear();
    for(std::uint32_t i = 0; i < count && r.ok && r.pos < end; i++)
    {
      std::remove_const_t<typename type::key_type> key{};
      typename type::mapped_type mapped{};
      if(!decode_value(r, key, end - r.pos))
        return false;
      if(!decode_value(r, mapped, end - r.pos))
        return false;
      v.insert(typename type::value_type(std::move(key), std::move(mapped)));
    }
    return r.ok;
  }
  else if constexpr(avnd::set_ish<type>)
  {
    std::uint32_t count{};
    if(!r.pod(count))
      return false;
    v.clear();
    for(std::uint32_t i = 0; i < count && r.ok && r.pos < end; i++)
    {
      std::remove_const_t<typename type::key_type> key{};
      if(!decode_value(r, key, end - r.pos))
        return false;
      v.insert(std::move(key));
    }
    return r.ok;
  }
  else if constexpr(byte_block<type>)
  {
    using elem = typename type::value_type;
    std::uint32_t count{};
    if(!r.pod(count))
      return false;
    // Trust the record size, not the count: a corrupt count must not make
    // us allocate or read out of bounds.
    if(count > (payload_size - sizeof(count)) / sizeof(elem))
      return false;
    v.resize(count);
    return r.raw(v.data(), std::size_t(count) * sizeof(elem));
  }
  else if constexpr(avnd::vector_ish<type>)
  {
    std::uint32_t count{};
    if(!r.pod(count))
      return false;
    v.clear();
    for(std::uint32_t i = 0; i < count && r.ok && r.pos < end; i++)
    {
      typename type::value_type elem{};
      if(!decode_value(r, elem, end - r.pos))
        return false;
      v.push_back(std::move(elem));
    }
    return r.ok;
  }
  else if constexpr(avnd::tuple_ish<type> && !std::is_trivially_copyable_v<type>)
  {
    bool ok = true;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((ok = ok && decode_value(r, std::get<I>(v), end - r.pos)), ...);
    }(std::make_index_sequence<std::tuple_size_v<type>>{});
    return ok;
  }
  else if constexpr(trivial_member<type>)
  {
    return r.pod(v);
  }
  else
  {
    bool nested_mismatch = false, nested_changed = false;
    return decode_fields(r, v, payload_size, nested_mismatch, nested_changed);
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

// Members no record covers keep the value they already hold, so the caller
// decides what "missing" means by pre-seeding defaults. An empty payload
// covers nothing and therefore succeeds without changing anything; whether
// a host handing over no state at all is an error belongs to the container
// above this one.
template <typename S>
  requires serializable<S>
load_result load(S& s, const std::uint8_t* data, std::size_t n)
{
  if(n == 0)
    return {.ok = true};
  if(!data)
    return {};
  reader r{data, n, 0, true};
  load_result res;
  res.ok = decode_fields(r, s, n, res.type_mismatch, res.layout_changed);
  return res;
}

template <typename S>
constexpr std::uint32_t layout_hash()
{
  return layout_hash_of<std::remove_cvref_t<S>>();
}

} // namespace avnd::state
