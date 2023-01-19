#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/export.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <lo/lo.h>

#include <cmath>
#include <algorithm>
#include <span>
#include <string>

namespace osc
{
struct liblo_send_binding
{
public:
  explicit liblo_send_binding(lo_address address, std::string_view root)
    :  address{address}
  {
    assert(root.length() < std::ssize(pattern));
    std::copy_n(root.data(), root.size(), pattern);
    pattern[root.size()] = '\0';
    pattern_end = pattern + root.size();
  }

  template<typename C>
  void set_pattern(const C&) {
    if constexpr(requires { C::osc_address(); })
    {
      constexpr std::string_view osc_addr = C::osc_address();
      std::copy_n(osc_addr.data(), osc_addr.size(), pattern_end);
      pattern_end[osc_addr.size()] = '\0';
    }
    else
    {
      pattern_end[0] = '/';
      auto addr = pattern_end + 1;
      constexpr std::string_view name = avnd::get_name<C>();
      std::copy_n(name.data(), name.size(), addr);
      addr[name.size()] = '\0';
    }
  }

  void add(int c, lo_message msg)
  {
    lo_message_add_int32(msg, c);
  }

  void add(float c, lo_message msg)
  {
    lo_message_add_float(msg, c);
  }

  void add(bool c, lo_message msg)
  {
    if(c)
      lo_message_add_true(msg);
    else
      lo_message_add_false(msg);
  }

  void add(std::string& c, lo_message msg)
  {
    lo_message_add_string(msg, c.data());
  }

  void add(std::string_view& c, lo_message msg)
  {
    lo_message_add_string(msg, c.data());
  }

  template<std::size_t N, typename V>
  void add(std::array<V, N>& c, lo_message msg)
  {
    for(auto& element : c)
      add(element, msg);
  }

  template<avnd::vector_ish V>
  void add(V& c, lo_message msg)
  {
    for(auto& element : c)
      add(element, msg);
  }

  template<avnd::set_ish V>
  void add(V& c, lo_message msg)
  {
    for(auto& element : c)
      add(element, msg);
  }

  template<avnd::map_ish V>
  void add(V& c, lo_message msg)
  {
    for(auto& element : c)
      add(element, msg);
  }

  template<avnd::optional_ish V>
  void add(V& c, lo_message msg)
  {
    if(c)
      add(*c, msg);
    else
      lo_message_add_nil(msg);
  }

  template<avnd::tuple_ish V>
  void add(V& c, lo_message msg)
  {
    std::apply([=] (auto&&... args) {
      (add(args, msg), ...);
    }, c);
  }

  template<avnd::variant_ish V>
  void add(V& c, lo_message msg)
  {
    visit([=] (auto& arg) { add(arg, msg); }, c);
  }

  template<typename V>
  void add(V& c, lo_message msg)
  {
  }


  void add(auto* c, lo_message msg) = delete;

  template<typename V>
  requires requires (V v) { std::begin(v) + std::size(v) != std::end(v); }
  void add(const V& vec, lo_message msg)
  {
    for(auto& value : vec)
      add(value, msg);
  }

  void push(auto& c)
  {
    auto msg = lo_message_new();

    add(c.value, msg);

    set_pattern(c);
    lo_send_message(address, pattern, msg);

    lo_message_free(msg);
  }

private:
  lo_address address{};
  char pattern[512] = {0};
  char* pattern_end{};
};

}
