#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wasm
{
/**
 * Wait-free SPSC ring buffer over a flat region of (shared) WASM linear memory.
 * Layout is byte-for-byte identical to padenot/ringbuf.js so JS and C++ agree:
 *   [0] read index (uint32), [4] write index (uint32), [8..] capacity bytes.
 * Free-running indices masked by capacity (must be power of two). Non-owning:
 * a view over a region typically backed by a SharedArrayBuffer on the JS side.
 */
class ring_buffer
{
public:
  static constexpr std::size_t header_size = 2 * sizeof(std::uint32_t);

  ring_buffer() noexcept = default;

  // total_bytes is the whole region including the 8-byte header.
  // (total_bytes - header_size) must be a power of two.
  ring_buffer(std::uint8_t* storage, std::size_t total_bytes) noexcept
      : m_header{reinterpret_cast<std::uint32_t*>(storage)}
      , m_data{storage + header_size}
      , m_capacity{static_cast<std::uint32_t>(total_bytes - header_size)}
  {
  }

  [[nodiscard]] std::uint32_t capacity() const noexcept { return m_capacity; }

  [[nodiscard]] std::uint32_t available_read() const noexcept
  {
    const std::uint32_t rd = read_index().load(std::memory_order_relaxed);
    const std::uint32_t wr = write_index().load(std::memory_order_acquire);
    return wr - rd;
  }

  [[nodiscard]] std::uint32_t available_write() const noexcept
  {
    return m_capacity - available_read();
  }

  // Pushes up to `len` bytes ; returns the number actually written.
  std::uint32_t push(const void* src, std::uint32_t len) noexcept
  {
    const std::uint32_t rd = read_index().load(std::memory_order_acquire);
    const std::uint32_t wr = write_index().load(std::memory_order_relaxed);

    const std::uint32_t free_space = m_capacity - (wr - rd);
    const std::uint32_t to_write = len < free_space ? len : free_space;
    if(to_write == 0)
      return 0;

    const std::uint32_t mask = m_capacity - 1;
    const std::uint32_t offset = wr & mask;
    const std::uint32_t first = (m_capacity - offset) < to_write
                                    ? (m_capacity - offset)
                                    : to_write;

    std::memcpy(m_data + offset, src, first);
    if(to_write > first)
      std::memcpy(m_data, static_cast<const std::uint8_t*>(src) + first, to_write - first);

    write_index().store(wr + to_write, std::memory_order_release);
    return to_write;
  }

  // Pops up to `len` bytes ; returns the number actually read.
  std::uint32_t pop(void* dst, std::uint32_t len) noexcept
  {
    const std::uint32_t rd = read_index().load(std::memory_order_relaxed);
    const std::uint32_t wr = write_index().load(std::memory_order_acquire);

    const std::uint32_t filled = wr - rd;
    const std::uint32_t to_read = len < filled ? len : filled;
    if(to_read == 0)
      return 0;

    const std::uint32_t mask = m_capacity - 1;
    const std::uint32_t offset = rd & mask;
    const std::uint32_t first = (m_capacity - offset) < to_read
                                    ? (m_capacity - offset)
                                    : to_read;

    std::memcpy(dst, m_data + offset, first);
    if(to_read > first)
      std::memcpy(static_cast<std::uint8_t*>(dst) + first, m_data, to_read - first);

    read_index().store(rd + to_read, std::memory_order_release);
    return to_read;
  }

  // Discards all readable bytes (consumer side reset).
  void clear() noexcept
  {
    read_index().store(
        write_index().load(std::memory_order_acquire), std::memory_order_release);
  }

  [[nodiscard]] bool valid() const noexcept { return m_header != nullptr; }

private:
  std::atomic_ref<std::uint32_t> read_index() const noexcept
  {
    return std::atomic_ref<std::uint32_t>{m_header[0]};
  }
  std::atomic_ref<std::uint32_t> write_index() const noexcept
  {
    return std::atomic_ref<std::uint32_t>{m_header[1]};
  }

  std::uint32_t* m_header{};
  std::uint8_t* m_data{};
  std::uint32_t m_capacity{};
};

}
