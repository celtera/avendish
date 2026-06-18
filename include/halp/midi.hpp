#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/container/small_vector.hpp>
#include <halp/modules.hpp>
#include <halp/static_string.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <type_traits>

HALP_MODULE_EXPORT
namespace halp
{

struct midi_msg
{
  boost::container::small_vector<uint8_t, 15> bytes;
  int64_t timestamp{};
};
struct midi_note_msg
{
  uint8_t bytes[8];
  int64_t timestamp{};
};

template <static_string lit, typename MessageType = midi_msg>
struct midi_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }

  boost::container::small_vector<MessageType, 2> midi_messages;

  operator auto &() noexcept { return midi_messages; }
  operator const auto &() const noexcept { return midi_messages; }

  auto size() const noexcept { return midi_messages.size(); }
  auto empty() const noexcept { return midi_messages.size() == 0; }

  auto begin() noexcept { return midi_messages.begin(); }
  auto end() noexcept { return midi_messages.end(); }
  auto cbegin() noexcept { return midi_messages.cbegin(); }
  auto cend() noexcept { return midi_messages.cend(); }
  auto begin() const noexcept { return midi_messages.begin(); }
  auto end() const noexcept { return midi_messages.end(); }
  auto cbegin() const noexcept { return midi_messages.cbegin(); }
  auto cend() const noexcept { return midi_messages.cend(); }

  auto& front() const noexcept { return midi_messages.front(); }
  auto& back() const noexcept { return midi_messages.back(); }
  auto& front() noexcept { return midi_messages.front(); }
  auto& back() noexcept { return midi_messages.back(); }

  auto& operator[](std::size_t i) const noexcept { return midi_messages[i]; }

  void push_back(const MessageType& msg) { midi_messages.push_back(msg); }
  void push_back(MessageType&& msg) { midi_messages.push_back(std::move(msg)); }
  template <typename... Args>
  void emplace_back(Args&&... t)
  {
    midi_messages.emplace_back(std::forward<Args>(t)...);
  }
};

template <static_string lit>
using midi_note_bus = midi_bus<lit, midi_note_msg>;

template <static_string lit, typename MessageType = midi_msg>
struct midi_out_bus : midi_bus<lit, MessageType>
{
  MessageType& note_on(uint8_t channel, uint8_t note, uint8_t velocity) noexcept
  {
    return create(make_command(message_type::NOTE_ON, channel), note, velocity);
  }

  MessageType& note_off(uint8_t channel, uint8_t note, uint8_t velocity) noexcept
  {
    return create(make_command(message_type::NOTE_OFF, channel), note, velocity);
  }

  MessageType& control_change(uint8_t channel, uint8_t control, uint8_t value) noexcept
  {
    return create(make_command(message_type::CONTROL_CHANGE, channel), control, value);
  }

  MessageType& program_change(uint8_t channel, uint8_t value) noexcept
  {
    return create(make_command(message_type::PROGRAM_CHANGE, channel), value);
  }

  MessageType& pitch_bend(uint8_t channel, int value) noexcept
  {
    return create(
        make_command(message_type::PITCH_BEND, channel), (unsigned char)(value & 0x7F),
        (uint8_t)((value >> 7) & 0x7F));
  }

  MessageType& pitch_bend(uint8_t channel, uint8_t lsb, uint8_t msb) noexcept
  {
    return create(make_command(message_type::PITCH_BEND, channel), lsb, msb);
  }

  MessageType& poly_pressure(uint8_t channel, uint8_t note, uint8_t value) noexcept
  {
    return create(make_command(message_type::POLY_PRESSURE, channel), note, value);
  }

  MessageType& aftertouch(uint8_t channel, uint8_t value) noexcept
  {
    return create(make_command(message_type::AFTERTOUCH, channel), value);
  }

private:
  enum class message_type : uint8_t
  {
    INVALID = 0x0,
    // Standard Message
    NOTE_OFF = 0x80,
    NOTE_ON = 0x90,
    POLY_PRESSURE = 0xA0,
    CONTROL_CHANGE = 0xB0,
    PROGRAM_CHANGE = 0xC0,
    AFTERTOUCH = 0xD0,
    PITCH_BEND = 0xE0,

    // System Common Messages
    SYSTEM_EXCLUSIVE = 0xF0,
    TIME_CODE = 0xF1,
    SONG_POS_POINTER = 0xF2,
    SONG_SELECT = 0xF3,
    RESERVED1 = 0xF4,
    RESERVED2 = 0xF5,
    TUNE_REQUEST = 0xF6,
    EOX = 0xF7,

    // System Realtime Messages
    TIME_CLOCK = 0xF8,
    RESERVED3 = 0xF9,
    START = 0xFA,
    CONTINUE = 0xFB,
    STOP = 0xFC,
    RESERVED4 = 0xFD,
    ACTIVE_SENSING = 0xFE,
    SYSTEM_RESET = 0xFF
  };
  static uint8_t make_command(const message_type type, const int channel) noexcept
  {
    // MIDI channels are 0..15 (low nibble of the status byte). The previous
    // upper bound `channel - 1` was both off-by-one and UB for channel == 0
    // (std::clamp requires lo <= hi).
    return (uint8_t)((uint8_t)type | (uint8_t)std::clamp(channel, 0, 15));
  }

  // Assign bytes whether MessageType::bytes is a resizable container
  // (e.g. small_vector, the default midi_msg) or a fixed C array (midi_note_msg,
  // which is not list-assignable).
  template <typename Bytes>
  static void assign_bytes(Bytes& bytes, std::initializer_list<uint8_t> vals) noexcept
  {
    if constexpr(std::is_array_v<Bytes>)
    {
      std::size_t i = 0;
      for(uint8_t v : vals)
        if(i < std::extent_v<Bytes>)
          bytes[i++] = v;
    }
    else
    {
      bytes.assign(vals);
    }
  }

  MessageType& create(uint8_t b0, uint8_t b1) noexcept
  {
    auto& m = this->midi_messages.emplace_back();
    assign_bytes(m.bytes, {b0, b1});
    return m;
  }
  MessageType& create(uint8_t b0, uint8_t b1, uint8_t b2) noexcept
  {
    auto& m = this->midi_messages.emplace_back();
    assign_bytes(m.bytes, {b0, b1, b2});
    return m;
  }
};
}
