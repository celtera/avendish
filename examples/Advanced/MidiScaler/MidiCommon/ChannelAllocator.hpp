#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace mtk
{
/**
 * @brief Gives each new note a MIDI channel of its own, from a set of
 * channels, so that per-channel messages (pitch bend, pressure) only affect
 * that note.
 *
 * A channel is free when none of its notes is held and its last note ended
 * at least `release_reserve` frames ago, so that the release tail of a
 * synth voice does not get the next note's pitch bend.
 */
struct channel_allocator
{
  enum class mode : uint8_t
  {
    round_robin, //!< next free channel after the last one used, wrapping
    ping_pong,   //!< as round robin, reversing direction at the ends
    lowest,      //!< lowest free channel
    random       //!< any free channel
  };

  //! Bit n set: channel n + 1 can be used.
  void set_channels(uint16_t mask) noexcept { m_mask = mask; }
  uint16_t channels() const noexcept { return m_mask; }
  void set_mode(mode m) noexcept { m_mode = m; }
  void set_release_reserve(int64_t frames) noexcept { m_reserve = frames; }

  bool usable(int ch) const noexcept { return ch >= 1 && ch <= 16 && (m_mask >> (ch - 1)) & 1; }

  bool is_free(int ch, int64_t now) const noexcept
  {
    const auto& c = m_ch[ch - 1];
    return c.held == 0 && (now - c.ended_at) >= m_reserve;
  }

  //! A free channel, or nothing when every usable channel is taken.
  //! `random01` draws in [0, 1) and is only used in random mode.
  template <typename Random>
  std::optional<int> allocate(int64_t now, Random&& random01) noexcept
  {
    int candidates[16];
    int n = 0;
    for(int ch = 1; ch <= 16; ch++)
      if(usable(ch) && is_free(ch, now))
        candidates[n++] = ch;
    if(n == 0)
      return std::nullopt;

    int chosen = candidates[0];
    switch(m_mode)
    {
      case mode::lowest:
        break;
      case mode::random:
        chosen = candidates[std::min(n - 1, int(random01() * n))];
        break;
      case mode::round_robin:
        chosen = next_after(candidates, n, m_last, +1, true);
        break;
      case mode::ping_pong: {
        int c = next_after(candidates, n, m_last, m_dir, false);
        if(c == 0)
        {
          m_dir = -m_dir;
          c = next_after(candidates, n, m_last, m_dir, false);
        }
        chosen = c != 0 ? c : candidates[0];
        break;
      }
    }

    take(chosen);
    return chosen;
  }

  std::optional<int> allocate(int64_t now) noexcept
  {
    return allocate(now, [] { return 0.; });
  }

  //! Marks a channel as playing one more note, whether it was allocated here
  //! or chosen by the caller (a surplus channel, say).
  void take(int ch) noexcept
  {
    if(ch < 1 || ch > 16)
      return;
    m_ch[ch - 1].held++;
    m_last = ch;
  }

  void release(int ch, int64_t now) noexcept
  {
    if(ch < 1 || ch > 16)
      return;
    auto& c = m_ch[ch - 1];
    if(c.held > 0)
      c.held--;
    if(c.held == 0)
      c.ended_at = now;
  }

  int held(int ch) const noexcept { return m_ch[(ch - 1) & 15].held; }

  //! The usable channel with no note held whose last note ended first, even
  //! if it is still in its release reserve; nothing if every one holds a note.
  std::optional<int> oldest_released() const noexcept
  {
    std::optional<int> best;
    for(int ch = 1; ch <= 16; ch++)
    {
      if(!usable(ch) || m_ch[ch - 1].held != 0)
        continue;
      if(!best || m_ch[ch - 1].ended_at < m_ch[*best - 1].ended_at)
        best = ch;
    }
    return best;
  }

  //! The n-th usable channel (0: the lowest), or nothing.
  std::optional<int> nth_usable(int n) const noexcept
  {
    for(int ch = 1; ch <= 16; ch++)
      if(usable(ch) && n-- == 0)
        return ch;
    return std::nullopt;
  }

  void reset() noexcept
  {
    m_ch = {};
    m_last = 0;
    m_dir = 1;
  }

private:
  //! The first candidate after `from` in direction `dir`; with `wrap`, starts
  //! over from the other end. Returns 0 when there is none.
  static int next_after(const int* cands, int n, int from, int dir, bool wrap) noexcept
  {
    if(dir > 0)
    {
      for(int i = 0; i < n; i++)
        if(cands[i] > from)
          return cands[i];
      return wrap ? cands[0] : 0;
    }
    else
    {
      for(int i = n - 1; i >= 0; i--)
        if(cands[i] < from)
          return cands[i];
      return wrap ? cands[n - 1] : 0;
    }
  }

  struct channel_state
  {
    int held{};
    int64_t ended_at{INT64_MIN / 2};
  };
  std::array<channel_state, 16> m_ch{};
  uint16_t m_mask{0xFFFF};
  int64_t m_reserve{};
  mode m_mode{mode::round_robin};
  int m_last{0};
  int m_dir{1};
};
}
