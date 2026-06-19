#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Minimal nlohmann-json-compatible JSON *builder* backed by yyjson, covering
// only the subset the `dump` backend needs: object/array building, an
// auto-vivifying operator[], scalar / string / string-range assignment, and
// pretty serialization.
//
// Rationale is compile time, not runtime: nlohmann/json's monolithic template
// header costs ~1.4s per translation unit, and every registered example builds
// one dump TU. yyjson is a C library whose header is declarations only (~0.05s
// per TU); the format machinery lives in a single yyjson.c compiled once for the
// whole project. The output is still standard JSON, read back unchanged by the
// nlohmann/inja-based maxref tooling.

#include <yyjson.h>

#include <concepts>
#include <cstdlib>
#include <deque>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>

namespace dump_json
{
class document;

// Types serializable as a single JSON scalar. Used both for direct assignment
// and to constrain the range overload so that ranges of non-scalar elements
// (pair/tuple/map/nested containers) fail the constraint cleanly — letting the
// dump's `if_possible(obj["default"] = ...)` skip them instead of hard-erroring
// deep inside an instantiation. Matching nlohmann's full type->JSON adapter set
// (map->object, recursive nesting) would require heavy recursive concepts, which
// defeats the compile-time goal; those exotic defaults are synthetic-only.
template <typename T>
concept json_scalar
    = std::integral<T> || std::floating_point<T> || std::is_enum_v<T>
      || std::convertible_to<T, std::string_view>;

// A handle to one node in the document's tree. Cheap to copy (a pointer pair);
// mutating it mutates the underlying yyjson node in place, matching the
// reference semantics of nlohmann::json's operator[].
class value
{
public:
  document* owner{};
  yyjson_mut_val* node{};

  value() = default;
  value(document* o, yyjson_mut_val* n)
      : owner{o}
      , node{n}
  {
  }

  value& operator=(bool b)
  {
    yyjson_mut_set_bool(node, b);
    return *this;
  }

  template <typename I>
    requires(std::integral<I> && !std::same_as<std::remove_cvref_t<I>, bool>)
  value& operator=(I x)
  {
    yyjson_mut_set_sint(node, static_cast<int64_t>(x));
    return *this;
  }

  template <typename F>
    requires std::floating_point<F>
  value& operator=(F x)
  {
    yyjson_mut_set_real(node, static_cast<double>(x));
    return *this;
  }

  template <typename E>
    requires std::is_enum_v<E>
  value& operator=(E x)
  {
    yyjson_mut_set_sint(node, static_cast<int64_t>(x));
    return *this;
  }

  value& operator=(std::string_view s);
  value& operator=(const char* s) { return *this = std::string_view(s); }
  value& operator=(const std::string& s) { return *this = std::string_view(s); }

  // A range of scalars (e.g. enum choices, a vector<float> default) -> JSON
  // array. Constrained to scalar elements so ranges of unsupported types are
  // rejected at the constraint (skipped by if_possible), not the body.
  template <typename R>
    requires requires(const R& r) {
      std::begin(r);
      std::end(r);
      requires json_scalar<std::remove_cvref_t<decltype(*std::begin(r))>>;
    } && (!std::convertible_to<R, std::string_view>)
  value& operator=(const R& range);

  // Auto-vivifying member access: turns this node into an object if needed and
  // returns a handle to member `key` (created as null on first access).
  value operator[](std::string_view key);

  // Array building: turns this node into an array if needed and appends.
  void ensure_array()
  {
    if(!yyjson_mut_is_arr(node))
      yyjson_mut_set_arr(node);
  }
  void push_back(const value& v);
  template <typename T>
  void push_back(const T& scalar);
};

class document
{
public:
  yyjson_mut_doc* doc{yyjson_mut_doc_new(nullptr)};
  // Stable backing storage for copied strings/keys (yyjson_mut_set_strn / strn
  // do not copy). std::deque never relocates elements, so the pointers stay valid
  // for the document's lifetime.
  std::deque<std::string> arena;

  document() = default;
  document(const document&) = delete;
  document& operator=(const document&) = delete;
  ~document() { yyjson_mut_doc_free(doc); }

  std::string_view intern(std::string_view s) { return arena.emplace_back(s); }

  value root()
  {
    auto* r = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, r);
    return {this, r};
  }

  // A fresh, unlinked null node, to be filled and then push_back'd into an array.
  value make_node() { return {this, yyjson_mut_null(doc)}; }

  std::string dump() const
  {
    std::size_t len = 0;
    char* s = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &len);
    std::string out = s ? std::string(s, len) : std::string{};
    if(s)
      free(s);
    return out;
  }
};

inline value& value::operator=(std::string_view s)
{
  auto sv = owner->intern(s);
  yyjson_mut_set_strn(node, sv.data(), sv.size());
  return *this;
}

inline value value::operator[](std::string_view key)
{
  if(!yyjson_mut_is_obj(node))
    yyjson_mut_set_obj(node);
  auto k = owner->intern(key);
  if(auto* existing = yyjson_mut_obj_getn(node, k.data(), k.size()))
    return {owner, existing};
  auto* kv = yyjson_mut_strn(owner->doc, k.data(), k.size());
  auto* vv = yyjson_mut_null(owner->doc);
  yyjson_mut_obj_add(node, kv, vv);
  return {owner, vv};
}

inline void value::push_back(const value& v)
{
  if(!yyjson_mut_is_arr(node))
    yyjson_mut_set_arr(node);
  yyjson_mut_arr_append(node, v.node);
}

template <typename T>
inline void value::push_back(const T& scalar)
{
  value tmp = owner->make_node();
  tmp = scalar;
  push_back(tmp);
}

template <typename R>
  requires requires(const R& r) {
    std::begin(r);
    std::end(r);
    requires json_scalar<std::remove_cvref_t<decltype(*std::begin(r))>>;
  } && (!std::convertible_to<R, std::string_view>)
inline value& value::operator=(const R& range)
{
  yyjson_mut_set_arr(node);
  for(const auto& e : range)
    push_back(e);
  return *this;
}
}
