#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <libremidi/message.hpp>

#include <array>
#include <cstdint>

namespace mtk
{
/**
 * @brief Remembers which output notes each held input note started, so that
 * every note-on an object sends gets exactly one note-off.
 *
 * Output notes are reference-counted per channel and pitch: when two input
 * notes produce the same output note, it is started once and ended when the
 * last of them is released, so that releasing one never cuts the other.
 *
 * Fixed capacity, never allocates: when full, the oldest held note is
 * released to make room.
 */
template <int MaxHeld = 128, int MaxOutputs = 12>
struct note_tracker
{
  struct out_note
  {
    uint8_t channel{}; //!< 1 - 16
    uint8_t pitch{};
  };

  struct held
  {
    uint8_t channel{}; //!< input channel, 1 - 16
    uint8_t pitch{};   //!< input pitch
    uint8_t count{};
    std::array<out_note, MaxOutputs> outputs{};
  };

  //! Starts tracking an input note. Emits note-offs through `emit` if the
  //! oldest note has to be released to make room.
  template <typename Emit>
  held& begin(int channel, int pitch, int64_t ts, Emit&& emit) noexcept
  {
    if(m_count == MaxHeld)
      release_at(0, ts, 0, emit);

    held& h = m_held[m_count++];
    h.channel = uint8_t(channel);
    h.pitch = uint8_t(pitch);
    h.count = 0;
    return h;
  }

  //! Sends a note-on for an output note of `h`, unless that note already sounds
  //! for another held note. Extra notes beyond MaxOutputs are ignored.
  template <typename Emit>
  void play(held& h, int channel, int pitch, int velocity, int64_t ts, Emit&& emit) noexcept
  {
    if(pitch < 0 || pitch > 127 || channel < 1 || channel > 16 || h.count >= MaxOutputs)
      return;
    velocity = velocity < 1 ? 1 : (velocity > 127 ? 127 : velocity);
    h.outputs[h.count++] = {uint8_t(channel), uint8_t(pitch)};
    if(m_sounding[channel - 1][pitch]++ == 0)
    {
      auto m = libremidi::channel_events::note_on(channel, pitch, velocity);
      m.timestamp = ts;
      emit(std::move(m));
    }
  }

  //! Releases the oldest held note matching the input note. Returns false if
  //! none matched: the caller usually passes such a note-off through.
  template <typename Emit>
  bool end(int channel, int pitch, int64_t ts, int velocity, Emit&& emit) noexcept
  {
    for(int i = 0; i < m_count; i++)
    {
      if(m_held[i].channel == channel && m_held[i].pitch == pitch)
      {
        release_at(i, ts, velocity, emit);
        return true;
      }
    }
    return false;
  }

  template <typename Emit>
  void end_all(int64_t ts, Emit&& emit) noexcept
  {
    while(m_count > 0)
      release_at(m_count - 1, ts, 0, emit);
  }

  //! Whether an input note is held.
  bool holds(int channel, int pitch) const noexcept
  {
    for(int i = 0; i < m_count; i++)
      if(m_held[i].channel == channel && m_held[i].pitch == pitch)
        return true;
    return false;
  }

  //! The oldest held instance of an input note, or nullptr.
  held* find(int channel, int pitch) noexcept
  {
    for(int i = 0; i < m_count; i++)
      if(m_held[i].channel == channel && m_held[i].pitch == pitch)
        return &m_held[i];
    return nullptr;
  }

  int size() const noexcept { return m_count; }
  const held& operator[](int i) const noexcept { return m_held[i]; }

  //! Number of held notes currently producing this output note.
  int sounding(int channel, int pitch) const noexcept
  {
    return m_sounding[(channel - 1) & 15][pitch & 127];
  }

private:
  template <typename Emit>
  void release_at(int index, int64_t ts, int velocity, Emit& emit) noexcept
  {
    held& h = m_held[index];
    for(int k = 0; k < h.count; k++)
    {
      auto& s = m_sounding[h.outputs[k].channel - 1][h.outputs[k].pitch];
      if(s > 0 && --s == 0)
      {
        auto m = libremidi::channel_events::note_off(
            h.outputs[k].channel, h.outputs[k].pitch, velocity);
        m.timestamp = ts;
        emit(std::move(m));
      }
    }
    for(int i = index; i + 1 < m_count; i++)
      m_held[i] = m_held[i + 1];
    m_count--;
  }

  std::array<held, MaxHeld> m_held{};
  int m_count{};
  std::array<std::array<uint16_t, 128>, 16> m_sounding{};
};
}
