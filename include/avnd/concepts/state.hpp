#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>

#include <cstddef>
#include <cstdint>

namespace avnd
{
/**
 * Host state (session persistence) support: processors may expose an opaque,
 * processor-versioned byte blob that plug-in bindings persist in DAW
 * sessions alongside parameter values.
 *
 * See STATE_SUPPORT_DESIGN.md. Accepted spellings:
 *
 *   // Save, container-return form: any contiguous byte container
 *   std::vector<uint8_t> save_state();
 *
 *   // Save, writer form (zero-allocation streaming)
 *   void save_state(avnd::state_writer auto& write); // write(ptr, size)
 *
 *   // Load: pointer + size (byte-like pointee), or a span-ish of bytes.
 *   // Returning bool: false = blob rejected, no partial application.
 *   bool load_state(const uint8_t* data, std::size_t n);
 *   bool load_state(std::span<const uint8_t> bytes);
 *
 * The blob comes back from arbitrary session files: processors must
 * validate it (magic/version/checksum) and fail cleanly.
 */

// A byte-like element: char, unsigned char, std::byte...
template <typename T>
concept byte_like = (sizeof(T) == 1) && !std::is_same_v<std::remove_cv_t<T>, bool>;

// A contiguous container of bytes, e.g. std::vector<uint8_t>, std::string.
template <typename T>
concept byte_container = requires(const T& t) {
                           { t.data() } -> std::convertible_to<const void*>;
                           { t.size() } -> std::convertible_to<std::size_t>;
                         } && byte_like<std::remove_pointer_t<decltype(std::declval<const T&>().data())>>;

// The sink passed to the writer form of save_state.
template <typename T>
concept state_writer
    = requires(T t, const void* p, std::size_t n) { t(p, n); };

// Minimal writer used for detection of the writer form.
struct dummy_state_writer
{
  void operator()(const void*, std::size_t) const noexcept { }
};

// ---- save spellings -------------------------------------------------------
template <typename T>
concept has_save_state_container = requires(T t) {
                                     { t.save_state() } -> byte_container;
                                   };

template <typename T>
concept has_save_state_writer
    = requires(T t, dummy_state_writer w) { t.save_state(w); };

template <typename T>
concept has_save_state = has_save_state_container<T> || has_save_state_writer<T>;

// ---- load spellings -------------------------------------------------------
template <typename T>
concept has_load_state_ptr
    = requires(T t, const std::uint8_t* d, std::size_t n) { t.load_state(d, n); }
      || requires(T t, const char* d, std::size_t n) { t.load_state(d, n); }
      || requires(T t, const std::byte* d, std::size_t n) { t.load_state(d, n); };

// Span-ish overload: anything constructible from (const uint8_t*, size_t)
// that the object accepts, e.g. std::span<const uint8_t>.
template <typename T>
concept has_load_state_span = requires(T t, const std::uint8_t* d, std::size_t n) {
                                t.load_state({d, n});
                              };

template <typename T>
concept has_load_state = has_load_state_ptr<T> || has_load_state_span<T>;

// ---- the feature ----------------------------------------------------------
template <typename T>
concept has_custom_state = has_save_state<T> && has_load_state<T>;

}
