#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Minimal nlohmann-json-compatible JSON builder backed by yyjson, covering the
// subset the dump backend needs. yyjson keeps the per-TU include cost low.

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

// Types serializable as a single JSON scalar. Constrains the range overload so
// non-scalar element ranges fail the constraint cleanly (skipped by if_possible).
template <typename T>
concept json_scalar
    = std::integral<T> || std::floating_point<T> || std::is_enum_v<T>
      || std::convertible_to<T, std::string_view>;

// Handle to one node in the document tree; mutates the yyjson node in place.
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

  // A range of scalars -> JSON array.
  template <typename R>
    requires requires(const R& r) {
      std::begin(r);
      std::end(r);
      requires json_scalar<std::remove_cvref_t<decltype(*std::begin(r))>>;
    } && (!std::convertible_to<R, std::string_view>)
  value& operator=(const R& range);

  // Auto-vivifying member access: makes this node an object, returns member key.
  value operator[](std::string_view key);

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
  // Stable backing storage for keys/strings: yyjson_mut_*_strn do not copy, and
  // deque never relocates, so pointers stay valid for the document's lifetime.
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

  // A fresh, unlinked null node, to be filled and then push_back'd.
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
