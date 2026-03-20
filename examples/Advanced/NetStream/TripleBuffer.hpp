#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <atomic>
#include <span>
#include <vector>

namespace netstream
{

template <typename T, typename Container = std::vector<T>>
class TripleBuffer
{
  static_assert(
      std::is_nothrow_swappable_v<Container>, "Container must be nothrow swappable");

private:
  static constexpr uint8_t INDEX_MASK = 0x03;
  static constexpr uint8_t DIRTY_BIT = 0x04;

  struct alignas(64) Slot
  {
    Container data;

    Slot() = default;

    template <typename U>
    explicit Slot(U&& val)
        : data(std::forward<U>(val))
    {
    }
  };

  std::array<Slot, 3> buffers_;

  alignas(64) std::atomic<uint8_t> mid_state_;
  alignas(64) uint8_t write_idx_;
  alignas(64) uint8_t read_idx_;

  TripleBuffer(const TripleBuffer&) = delete;
  TripleBuffer& operator=(const TripleBuffer&) = delete;
  TripleBuffer(TripleBuffer&&) = delete;
  TripleBuffer& operator=(TripleBuffer&&) = delete;

public:
  TripleBuffer() noexcept(std::is_nothrow_default_constructible_v<Container>)
      : buffers_{}
      , mid_state_{1}
      , write_idx_{0}
      , read_idx_{2}
  {
  }

  // Pre-allocate each slot with a given capacity.
  explicit TripleBuffer(std::size_t initial_capacity)
      : buffers_{}
      , mid_state_{1}
      , write_idx_{0}
      , read_idx_{2}
  {
    for(auto& slot : buffers_)
      slot.data.reserve(initial_capacity);
  }

  // Direct access to the write slot. The producer writes into this
  // container however it likes, then calls publish().
  Container& write_buffer() noexcept { return buffers_[write_idx_].data; }

  // Publish the current write buffer: swap it into the middle slot
  // and pick up a new (stale) write slot.
  void publish() noexcept
  {
    const uint8_t new_mid = static_cast<uint8_t>(write_idx_ | DIRTY_BIT);
    const uint8_t old_mid = mid_state_.exchange(new_mid, std::memory_order_acq_rel);
    write_idx_ = old_mid & INDEX_MASK;
  }

  // Convenience: assign from an iterator range, then publish.
  template <typename InputIt>
  void produce(InputIt first, InputIt last)
  {
    buffers_[write_idx_].data.assign(first, last);
    publish();
  }

  // Convenience: assign from a span, then publish.
  void produce(std::span<const T> src)
  {
    buffers_[write_idx_].data.assign(src.begin(), src.end());
    publish();
  }

  // Attempt to consume. Returns true if new data was swapped in.
  // After a successful consume(), read_span() / read_buffer()
  // reflect the new data.
  bool consume() noexcept
  {
    const uint8_t state = mid_state_.load(std::memory_order_acquire);
    if(!(state & DIRTY_BIT))
      return false;

    const uint8_t new_mid = read_idx_;
    const uint8_t old_mid = mid_state_.exchange(new_mid, std::memory_order_acq_rel);
    read_idx_ = old_mid & INDEX_MASK;
    return true;
  }

  // View of the consumer's current slot.
  std::span<const T> read_span() const noexcept
  {
    const auto& c = buffers_[read_idx_].data;
    return {c.data(), c.size()};
  }

  const Container& read_buffer() const noexcept { return buffers_[read_idx_].data; }

  bool has_new_data() const noexcept
  {
    return mid_state_.load(std::memory_order_acquire) & DIRTY_BIT;
  }
};

} // namespace netstream
