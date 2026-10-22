#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

#include <array>
#include <cstdint>

namespace mtk
{
/**
 * @brief Up to 8 stored controller values on one channel, sent on demand.
 *
 * Incoming MIDI passes through. In Takeover mode, once a slot has been sent,
 * incoming values of the same controller on the same channel are held back
 * until they reach or cross the stored value, so that moving a physical knob
 * does not make the parameter jump away from the recalled value.
 */
struct MidiCCSnapshot
{
  halp_meta(name, "Midi CC snapshot")
  halp_meta(c_name, "midi_cc_snapshot")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(description, "Send up to 8 stored controller values on a channel")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-cc-snapshot.html")
  halp_meta(uuid, "e941cff0-8ebb-4259-a146-29ce10125791")

  static constexpr int slots = 8;

  enum Mode
  {
    Immediate,
    Takeover
  };

  template <halp::static_string Name, int Init>
  struct cc_number : halp::spinbox_i32<Name, halp::range{0, 127, Init}>
  {
    halp_meta(description, "Controller number of this slot")
  };
  template <halp::static_string Name>
  struct cc_value : halp::hslider_i32<Name, halp::range{0, 127, 0}>
  {
    halp_meta(description, "Value sent for this slot")
  };

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
    struct : halp::impulse_button<"Send">
    {
      halp_meta(description, "Send every active slot")
    } send;
    struct : halp::toggle<"Send on change">
    {
      halp_meta(description, "Also send a slot whenever its number or value changes")
    } on_change;
    struct : halp::enum_t<Mode, "Mode">
    {
      halp_meta(
          description,
          "Takeover: after a slot is sent, incoming values of that controller are "
          "held back until they reach the stored value")
    } mode;
    struct : halp::spinbox_i32<"Channel", halp::range{1, 16, 1}>
    {
      halp_meta(description, "Channel of the controllers sent")
    } channel;
    struct : halp::spinbox_i32<"Slots", halp::range{1, slots, slots}>
    {
      halp_meta(description, "Number of active slots")
    } count;

    cc_number<"CC 1", 20> cc1;
    cc_value<"Value 1"> value1;
    cc_number<"CC 2", 21> cc2;
    cc_value<"Value 2"> value2;
    cc_number<"CC 3", 22> cc3;
    cc_value<"Value 3"> value3;
    cc_number<"CC 4", 23> cc4;
    cc_value<"Value 4"> value4;
    cc_number<"CC 5", 24> cc5;
    cc_value<"Value 5"> value5;
    cc_number<"CC 6", 25> cc6;
    cc_value<"Value 6"> value6;
    cc_number<"CC 7", 26> cc7;
    cc_value<"Value 7"> value7;
    cc_number<"CC 8", 27> cc8;
    cc_value<"Value 8"> value8;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
  } outputs;

  void stop() noexcept { m_stop = true; }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical) noexcept
  {
    if(m_stop)
    {
      m_stop = false;
      m_locked = {};
    }

    const std::array<int, slots> ccs{inputs.cc1,  inputs.cc2, inputs.cc3,
                                     inputs.cc4,  inputs.cc5, inputs.cc6,
                                     inputs.cc7,  inputs.cc8};
    const std::array<int, slots> values{inputs.value1, inputs.value2, inputs.value3,
                                        inputs.value4, inputs.value5, inputs.value6,
                                        inputs.value7, inputs.value8};
    const int count = std::clamp(int(inputs.count), 1, slots);
    const int channel = std::clamp(int(inputs.channel), 1, 16);

    for(int i = 0; i < slots; i++)
    {
      const bool changed = m_initialized
                           && (ccs[i] != m_last_cc[i] || values[i] != m_last_value[i]
                               || channel != m_last_channel);
      if(i < count && (inputs.send || (inputs.on_change && changed)))
        send_slot(i, channel, ccs[i], values[i]);
      m_last_cc[i] = ccs[i];
      m_last_value[i] = values[i];
    }
    m_last_channel = channel;
    m_initialized = true;

    for(const auto& m : inputs.midi)
    {
      if(pass(m, channel, count, ccs, values))
        outputs.midi.push_back(m);
    }
  }

private:
  std::array<int, slots> m_last_cc{}, m_last_value{};
  int m_last_channel{};
  //! In Takeover mode, slots whose incoming controller is held back.
  std::array<bool, slots> m_locked{};
  //! Side of the stored value the last held-back value was on: -1, 0 or 1.
  std::array<int8_t, slots> m_side{};
  bool m_initialized{};
  bool m_stop{};

  void send_slot(int i, int channel, int cc, int value)
  {
    auto m = libremidi::channel_events::control_change(
        channel, std::clamp(cc, 0, 127), std::clamp(value, 0, 127));
    m.timestamp = 0;
    outputs.midi.push_back(m);
    m_locked[i] = inputs.mode == Takeover;
    m_side[i] = 0;
  }

  bool pass(
      const libremidi::message& m, int channel, int count,
      const std::array<int, slots>& ccs, const std::array<int, slots>& values) noexcept
  {
    if(inputs.mode != Takeover || m.size() < 3 || (m.bytes[0] & 0xF0) != 0xB0
       || channel_of(m) != channel)
      return true;

    bool keep = true;
    for(int i = 0; i < count; i++)
    {
      if(!m_locked[i] || ccs[i] != m.bytes[1])
        continue;
      const int v = m.bytes[2];
      const int side = v < values[i] ? -1 : (v > values[i] ? 1 : 0);
      if(side == 0 || (m_side[i] != 0 && side != m_side[i]))
      {
        m_locked[i] = false;
        continue;
      }
      m_side[i] = int8_t(side);
      keep = false;
    }
    return keep;
  }
};
}
