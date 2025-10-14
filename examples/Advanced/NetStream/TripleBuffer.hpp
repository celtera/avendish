#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <atomic>
#include <new>
#include <vector>

namespace netstream
{

/**
 * Lock-free triple buffer for passing data between threads.
 *
 * The triple buffer allows one thread to write while another reads,
 * with a third buffer for swapping. This avoids locks in the critical path.
 *
 * Thread safety:
 * - One producer thread calls write()
 * - One consumer thread calls read() and new_data_available()
 * - Uses atomic operations for synchronization
 */
template <typename T>
class TripleBuffer
{
public:
  TripleBuffer() = default;

  // Producer: Write new data (called from I/O thread)
  void write(T&& data)
  {
    // Write to the current write buffer
    int write_idx = m_write_index.load(std::memory_order_relaxed);
    std::swap(m_buffers[write_idx], data);

    // Swap write buffer with swap buffer
    int swap_idx = m_swap_index.load(std::memory_order_relaxed);
    m_write_index.store(swap_idx, std::memory_order_relaxed);
    m_swap_index.store(write_idx, std::memory_order_release);

    // Signal that new data is available
    m_new_data.store(true, std::memory_order_release);
  }

  // Consumer: Check if new data is available (called from processing thread)
  bool new_data_available() const noexcept
  {
    return m_new_data.load(std::memory_order_acquire);
  }

  // Consumer: Read the latest data (called from processing thread)
  // Returns true if new data was read
  bool read(T& out)
  {
    if(!m_new_data.load(std::memory_order_acquire))
      return false;

    // Swap read buffer with swap buffer
    int read_idx = m_read_index.load(std::memory_order_relaxed);
    int swap_idx = m_swap_index.load(std::memory_order_acquire);

    m_read_index.store(swap_idx, std::memory_order_relaxed);
    m_swap_index.store(read_idx, std::memory_order_relaxed);

    // Mark data as read
    m_new_data.store(false, std::memory_order_relaxed);

    // Copy the data
    auto& buf = m_buffers[m_read_index.load(std::memory_order_relaxed)];
    std::swap(out, buf);
    return true;
  }

  // Consumer: Get reference to current read buffer without copying
  // Only call this after checking new_data_available()
  const T& read_ref()
  {
    if(m_new_data.load(std::memory_order_acquire))
    {
      // Swap read buffer with swap buffer
      int read_idx = m_read_index.load(std::memory_order_relaxed);
      int swap_idx = m_swap_index.load(std::memory_order_acquire);

      m_read_index.store(swap_idx, std::memory_order_relaxed);
      m_swap_index.store(read_idx, std::memory_order_relaxed);

      m_new_data.store(false, std::memory_order_relaxed);
    }

    return m_buffers[m_read_index.load(std::memory_order_relaxed)];
  }

private:
  T m_buffers[3];                             // Three buffers
  alignas(std::hardware_destructive_interference_size) std::atomic<int> m_write_index{0};          // Current write buffer index
  alignas(std::hardware_destructive_interference_size) std::atomic<int> m_read_index{1};           // Current read buffer index
  alignas(std::hardware_destructive_interference_size) std::atomic<int> m_swap_index{2};           // Swap buffer index
  alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_new_data{false};        // New data available flag
};

} // namespace netstream
