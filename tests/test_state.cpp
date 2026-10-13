/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

// Static detection tests for the custom-state concepts (concepts/state.hpp)
// and the normalizing wrappers (wrappers/state.hpp).

#include <avnd/concepts/state.hpp>
#include <avnd/wrappers/state.hpp>

#include <cstring>
#include <span>
#include <string>
#include <vector>

// ---- accepted spellings ---------------------------------------------------

// The most common spelling: vector return + (pointer, size) load.
struct state_vector_ptr
{
  std::vector<uint8_t> save_state() { return {1, 2, 3}; }
  bool load_state(const uint8_t*, size_t) { return true; }
};
static_assert(avnd::has_save_state_container<state_vector_ptr>);
static_assert(avnd::has_load_state_ptr<state_vector_ptr>);
static_assert(avnd::has_custom_state<state_vector_ptr>);

// String container + char pointer load.
struct state_string_char
{
  std::string save_state() { return "hello"; }
  bool load_state(const char*, size_t) { return true; }
};
static_assert(avnd::has_custom_state<state_string_char>);

// Writer-callback save + span load, void-returning load (cannot fail).
struct state_writer_span
{
  void save_state(avnd::state_writer auto& write)
  {
    const uint8_t bytes[4] = {9, 9, 9, 9};
    write(bytes, 2);
    write(bytes, 2); // multiple calls concatenate
  }
  void load_state(std::span<const uint8_t>) { }
};
static_assert(avnd::has_save_state_writer<state_writer_span>);
static_assert(avnd::has_custom_state<state_writer_span>);

// std::byte flavour.
struct state_bytes
{
  std::vector<std::byte> save_state() { return {}; }
  bool load_state(const std::byte*, size_t) { return true; }
};
static_assert(avnd::has_custom_state<state_bytes>);

// ---- rejected shapes ------------------------------------------------------

struct no_state
{
  void operator()() { }
};
static_assert(!avnd::has_save_state<no_state>);
static_assert(!avnd::has_load_state<no_state>);
static_assert(!avnd::has_custom_state<no_state>);

// Save without load (or vice versa) is not the feature.
struct only_save
{
  std::vector<uint8_t> save_state() { return {}; }
};
static_assert(avnd::has_save_state<only_save>);
static_assert(!avnd::has_custom_state<only_save>);

struct only_load
{
  bool load_state(const uint8_t*, size_t) { return true; }
};
static_assert(!avnd::has_custom_state<only_load>);

// A non-byte container return does not count as state.
struct wrong_save
{
  std::vector<int> save_state() { return {}; }
  bool load_state(const uint8_t*, size_t) { return true; }
};
static_assert(!avnd::has_save_state_container<wrong_save>);

// ---- wrapper behavior (runtime, exercised by the linker-visible fn) -------

bool state_wrappers_smoke()
{
  // Container save -> sink.
  std::vector<uint8_t> collected;
  auto sink = [&](const void* d, std::size_t n) {
    auto b = static_cast<const uint8_t*>(d);
    collected.insert(collected.end(), b, b + n);
  };

  state_vector_ptr a;
  avnd::save_state(a, sink);
  bool ok = collected == std::vector<uint8_t>{1, 2, 3};

  // Writer save concatenates.
  collected.clear();
  state_writer_span w;
  avnd::save_state(w, sink);
  ok &= collected.size() == 4;

  // Load normalization across spellings.
  const uint8_t data[3] = {5, 6, 7};
  ok &= avnd::load_state(a, data, 3);
  state_string_char s;
  ok &= avnd::load_state(s, data, 3);
  ok &= avnd::load_state(w, data, 3); // void load -> success
  state_bytes b;
  ok &= avnd::load_state(b, data, 3);
  return ok;
}
