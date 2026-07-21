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
 *  - a member whose type changed: the record no longer matches the member
 *    at that position -- including an int becoming a float, which the
 *    scalar kind distinguishes despite the identical width -- so it is
 *    skipped instead of being reinterpreted.
 *
 * What it cannot see is a change that leaves the layout identical -- two
 * members of the same type swapped, or a member's *meaning* changed. The
 * layout hash catches the first class (any change of field count, order or
 * shape changes it); the second is what `state_version` and `migrate_state`
 * are for.
 *
 * A record that does not describe the member at its position is reported as
 * a type mismatch, which tells the caller the remaining positions cannot be
 * trusted; it refuses the load unless the processor declared a migration.
 * Appending members and dropping trailing ones are not mismatches.
 *
 * Supported members, recursively: trivially-copyable types, contiguous
 * containers of them (std::string, std::vector<int>, ...), aggregates, and
 * lists of any of those.
 *
 * Byte order is native: writer and reader are the same binary. Sizes and
 * tags are fixed-width so a foreign-endian reader can at least skip records
 * rather than misparse them.
 */

#include <boost/pfr.hpp>

#include <array>
#include <tuple>

#include <cstdint>
#include <cstring>
#include <string>
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

// A type that only borrows its data: trivially copyable, so it would be
// written out as raw bytes, and those bytes are an address that means
// nothing in the process that reads them back.
template <typename T>
concept view_like = requires(const T& t) {
  t.data();
  { t.size() } -> std::convertible_to<std::size_t>;
} && std::is_pointer_v<decltype(std::declval<const T&>().data())>
     && !requires(T& t) { t.resize(std::size_t{}); };

template <typename T>
constexpr bool borrows_memory();

template <typename T, std::size_t... I>
constexpr bool any_member_borrows(std::index_sequence<I...>)
{
  return (borrows_memory<boost::pfr::tuple_element_t<I, T>>() || ...);
}

// Persisted state has to own what it describes. Pointers, references and
// views survive a memcpy syntactically and mean nothing once reloaded, so
// they are refused rather than written into a session file.
// std::array, pair and tuple carry a std::tuple_size, which the aggregate
// reflection cannot decompose on every compiler. They are handled through
// the standard interface instead of being taken apart field by field.
template <typename T>
concept tuple_like = requires { std::tuple_size<std::remove_cvref_t<T>>::value; };

// std::array is the one tuple-like type worth supporting: it is a fixed
// block of elements and, when they are trivially copyable, it is one too.
// Whether std::pair and std::tuple are trivially copyable differs between
// standard libraries, so accepting them on that basis would make a state
// struct compile on one toolchain and not another. They are refused
// everywhere instead.
template <typename T>
inline constexpr bool is_std_array_v = false;
template <typename T, std::size_t N>
inline constexpr bool is_std_array_v<std::array<T, N>> = true;

template <typename T>
constexpr bool borrows_memory()
{
  using type = std::remove_cvref_t<T>;
  if constexpr(std::is_pointer_v<type> || std::is_reference_v<type>)
    return true;
  else if constexpr(std::is_array_v<type>)
    return borrows_memory<std::remove_extent_t<type>>();
  else if constexpr(tuple_like<type>)
  {
    // Same element type throughout for std::array; for pair and tuple this
    // only inspects the first, which is enough to reject the obvious cases
    // -- they are not serializable anyway unless trivially copyable.
    if constexpr(std::tuple_size_v<type> > 0)
      return borrows_memory<std::tuple_element_t<0, type>>();
    else
      return false;
  }
  // An aggregate owns whatever it holds; look at what that is.
  else if constexpr(std::is_aggregate_v<type>)
    return any_member_borrows<type>(
        std::make_index_sequence<boost::pfr::tuple_size_v<type>>{});
  else if constexpr(view_like<type>)
    return true;
  else
    return false;
}

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
  if constexpr(borrows_memory<type>())
    return false;
  else if constexpr(tuple_like<type> && !is_std_array_v<type>)
    return false;
  else if constexpr(trivial_member<type>)
    return true;
  else if constexpr(contiguous_container<type>)
    return true;
  else if constexpr(list_container<type>)
    return serializable_impl<typename type::value_type>();
  // A tuple-like type that got this far is not trivially copyable, so it
  // would have to be taken apart -- which is exactly what the aggregate
  // reflection cannot do for them. std::array of non-trivial elements ends
  // up here, and is left to the processor's own save/load.
  else if constexpr(std::is_aggregate_v<type> && !tuple_like<type>)
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
// Layout hash: field count, order and shape. Two structs with the same hash
// are interchangeable as far as this format is concerned; any structural
// edit changes it.

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
  ((h = layout_hash_of<boost::pfr::tuple_element_t<I, T>>(h)), ...);
  return h;
}

// Size alone does not identify a scalar: an int and a float of the same
// width would hash the same, and swapping them would reinterpret one as the
// other. Encode what kind of scalar it is as well.
enum class scalar_kind : std::uint32_t
{
  boolean = 1,
  signed_int = 2,
  unsigned_int = 3,
  floating = 4,
  enumeration = 5,
  pointer = 6,
  other = 7
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
  else if constexpr(std::is_pointer_v<type>)
    return scalar_kind::pointer;
  else
    return scalar_kind::other;
}

template <typename T>
constexpr std::uint32_t layout_hash_of(std::uint32_t h)
{
  using type = std::remove_cvref_t<T>;
  h = hash_step(h, std::uint32_t(tag_of<type>()));
  if constexpr(trivial_member<type>)
  {
    h = hash_step(h, std::uint32_t(sizeof(type)));
    h = hash_step(h, std::uint32_t(kind_of<type>()));
    // A trivially-copyable aggregate is memcpy'd, but its shape still has to
    // be part of the hash: two structs of the same size are not the same
    // state.
    if constexpr(std::is_aggregate_v<type> && !std::is_array_v<type>)
      return layout_hash_fields<type>(
          h, std::make_index_sequence<boost::pfr::tuple_size_v<type>>{});
    return h;
  }
  else if constexpr(contiguous_container<type>)
  {
    using elem = std::remove_cvref_t<decltype(*std::declval<type&>().data())>;
    h = hash_step(h, std::uint32_t(sizeof(elem)));
    return hash_step(h, std::uint32_t(kind_of<elem>()));
  }
  else if constexpr(list_container<type>)
    return layout_hash_of<typename type::value_type>(h);
  else
    return layout_hash_fields<type>(
        hash_step(h, std::uint32_t(boost::pfr::tuple_size_v<type>)),
        std::make_index_sequence<boost::pfr::tuple_size_v<type>>{});
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

template <typename T>
void encode_fields(writer& w, const T& agg)
{
  w.pod(format_version);
  w.pod(layout_hash_of<T>());
  w.pod(std::uint16_t(boost::pfr::tuple_size_v<T>));

  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (
        [&] {
          w.pod(tag_of<boost::pfr::tuple_element_t<I, T>>());
          w.pod(std::uint8_t(kind_of<boost::pfr::tuple_element_t<I, T>>()));
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

  constexpr std::size_t fields = boost::pfr::tuple_size_v<T>;
  if(count != fields)
    changed = true;

  for(std::size_t index = 0; index < count && r.ok && r.pos < end; index++)
  {
    tag t{};
    std::uint8_t k{};
    std::uint32_t size{};
    if(!r.pod(t) || !r.pod(k) || !r.pod(size))
      return false;
    if(!r.has(size))
      return false;

    const std::size_t payload_start = r.pos;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (
          [&] {
            if(I != index)
              return;
            using field_t = boost::pfr::tuple_element_t<I, T>;
            // A record whose shape no longer matches the member at this
            // position keeps the member's default rather than being
            // reinterpreted.
            if(t != tag_of<field_t>() || k != std::uint8_t(kind_of<field_t>()))
            {
              mismatch = true;
              return;
            }
            decode_value(r, boost::pfr::get<I>(agg), size);
          }(),
          ...);
    }(std::make_index_sequence<fields>{});

    // Trailing records (members since removed), mismatched ones, and
    // partially-read ones all resume at the next record.
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
// covers nothing and therefore succeeds without changing anything; whether a
// host handing over no state at all is an error belongs to the container
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
