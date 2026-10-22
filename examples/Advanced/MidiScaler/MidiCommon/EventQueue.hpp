#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <libremidi/message.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace mtk
{
//! Frames covered by a duration in quarter notes at a given tempo.
inline int64_t quarters_to_frames(double quarters, double tempo, double rate) noexcept
{
  if(tempo <= 0.)
    tempo = 120.;
  return int64_t(quarters * 60. / tempo * rate + 0.5);
}

inline int64_t seconds_to_frames(double seconds, double rate) noexcept
{
  return int64_t(seconds * rate + 0.5);
}

/**
 * @brief MIDI messages waiting to be sent, dated in absolute frames.
 *
 * The owner keeps a frame counter `now` that advances by the tick's frame
 * count. Messages with the same date keep their scheduling order.
 * Fixed capacity: when full, a note-off evicts the latest other message,
 * since a lost note-off hangs a note while a lost note-on does not.
 */
template <int Capacity = 256>
struct event_queue
{
  struct event
  {
    int64_t at{};
    libremidi::message msg;
  };

  void schedule(int64_t at, const libremidi::message& m) noexcept
  {
    if(m_size == Capacity)
    {
      if(!is_note_off(m))
        return;
      int victim = -1;
      for(int i = m_size - 1; i >= 0; i--)
      {
        if(!is_note_off(m_events[i].msg))
        {
          victim = i;
          break;
        }
      }
      if(victim < 0)
        victim = m_size - 1;
      erase(victim);
    }

    int pos = m_size;
    while(pos > 0 && m_events[pos - 1].at > at)
      pos--;
    for(int i = m_size; i > pos; i--)
      m_events[i] = std::move(m_events[i - 1]);
    m_events[pos].at = at;
    m_events[pos].msg = m;
    m_events[pos].msg.timestamp = 0;
    m_size++;
  }

  //! Sends every message due before `now + frames`, with its offset in the
  //! tick as timestamp. Messages due in the past go out at offset 0.
  template <typename Send>
  void release(int64_t now, int frames, Send&& send) noexcept
  {
    if(frames <= 0)
      return;
    const int64_t limit = now + frames;
    int n = 0;
    while(n < m_size && m_events[n].at < limit)
    {
      auto msg = m_events[n].msg;
      msg.timestamp = std::clamp<int64_t>(m_events[n].at - now, 0, frames - 1);
      send(std::move(msg));
      n++;
    }
    if(n > 0)
    {
      for(int i = n; i < m_size; i++)
        m_events[i - n] = std::move(m_events[i]);
      m_size -= n;
    }
  }

  //! Sends the queued note-offs and drops everything else: what a stop or an
  //! all-notes-off needs so that nothing already started keeps sounding.
  template <typename Send>
  void flush_note_offs(int64_t ts, Send&& send) noexcept
  {
    for(int i = 0; i < m_size; i++)
    {
      if(is_note_off(m_events[i].msg))
      {
        auto msg = m_events[i].msg;
        msg.timestamp = ts;
        send(std::move(msg));
      }
    }
    m_size = 0;
  }

  //! Removes the events for which `pred` is true.
  template <typename Pred>
  void remove_if(Pred&& pred) noexcept
  {
    int w = 0;
    for(int r = 0; r < m_size; r++)
    {
      if(!pred(m_events[r]))
      {
        if(w != r)
          m_events[w] = std::move(m_events[r]);
        w++;
      }
    }
    m_size = w;
  }

  void clear() noexcept { m_size = 0; }
  int size() const noexcept { return m_size; }
  bool empty() const noexcept { return m_size == 0; }
  const event& operator[](int i) const noexcept { return m_events[i]; }

private:
  void erase(int i) noexcept
  {
    for(int k = i; k + 1 < m_size; k++)
      m_events[k] = std::move(m_events[k + 1]);
    m_size--;
  }

  std::array<event, Capacity> m_events{};
  int m_size{};
};
}
