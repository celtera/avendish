#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Advanced/MidiScaler/MidiCommon/NoteTracker.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace mtk
{
/**
 * @brief Turns one kind of MIDI message into another, rule based.
 *
 * A message of kind "What" that passes the input filter (channel, data byte
 * ranges) produces a message of kind "Into", whose channel and data bytes come
 * from the selected sources. Sources in 1-16 (channels) and sources in 0-127
 * (data bytes, arguments) are scaled linearly when they feed the other range.
 * Missing data bytes read as 0. Pitch bend and song position use byte 1 as
 * the LSB and byte 2 as the MSB.
 *
 * Note pairing, so that no note hangs:
 * - "What" Note on also owns the note-off of each note-on it matched: when
 *   the note-on was replaced or dropped, its note-off is removed too.
 * - A note-off whose note-on went out unchanged is never removed: in Replace
 *   and Drop modes it is kept (Replace still adds the produced message).
 * - "Into" Note on: each produced note belongs to its source, identified by
 *   kind, channel, and note or controller number. It ends when that source
 *   ends: the note-off of a note-on, a controller, poly or channel pressure
 *   value of 0, or a produced velocity of 0. A source that has no end
 *   (program change, clock, start) ends its previous note when it fires again.
 * - CC 120 and 123 end every produced note and pass through untransformed.
 */
struct MidiTransform
{
  halp_meta(name, "Midi transform")
  halp_meta(c_name, "midi_transform")
  halp_meta(category, "Midi/Routing")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Turn one kind of MIDI message into another, with the channel and data "
      "bytes taken from the input or from arguments")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-transform.html")
  halp_meta(uuid, "1680d787-cb5d-4df3-8783-8bca4c3cad27")
  halp_flag(process_exec);

  enum What
  {
    NoteOn,
    NoteOff,
    CC,
    ProgramChange,
    ChannelPressure,
    PolyPressure,
    PitchBend,
    TimeCode,
    SongPosition,
    SongSelect,
    TuneRequest,
    Clock,
    Start,
    Continue,
    Stop,
    ActiveSensing,
    Reset,
    Sysex
  };
  static constexpr int into_count = 17; //!< every What but Sysex

  enum Mode
  {
    InsertBefore,
    InsertAfter,
    Replace,
    Drop
  };

  enum Source
  {
    Arg1,
    Arg2,
    ChannelArg,
    DataByte1,
    DataByte2,
    IncomingChannel
  };

  enum Test
  {
    Inside,
    Outside
  };

#define MTK_KIND_NAMES                                                              \
  "Note on", "Note off", "CC", "Program change", "Channel pressure",               \
      "Poly pressure", "Pitch bend", "MTC quarter frame", "Song position",        \
      "Song select", "Tune request", "Clock", "Start", "Continue", "Stop",        \
      "Active sensing", "Reset"
#define MTK_SOURCE_NAMES                                                            \
  "Value 1", "Value 2", "Fixed channel", "Data byte 1", "Data byte 2", "Incoming channel"

  struct ins
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    struct : halp::enum_t<What, "What">
    {
      enum widget
      {
        combobox
      };
      struct range
      {
        std::string_view values[18] = {MTK_KIND_NAMES, "Sysex"};
        What init{};
      };
      halp_meta(description, "Kind of message that is transformed")
    } what;

    struct : halp::enum_t<What, "Into">
    {
      enum widget
      {
        combobox
      };
      struct range
      {
        std::string_view values[into_count] = {MTK_KIND_NAMES};
        What init{};
      };
      halp_meta(description, "Kind of message produced")
    } into;

    struct : halp::enum_t<Mode, "Mode">
    {
      enum widget
      {
        combobox
      };
      struct range
      {
        std::string_view values[4] = {"Insert before", "Insert after", "Replace", "Drop"};
        Mode init{Replace};
      };
      halp_meta(
          description,
          "Insert: keep the original and add the produced message before or after "
          "it. Replace: send only the produced message. Drop: remove matching "
          "messages.")
    } mode;

    struct : halp::enum_t<Source, "Channel from">
    {
      enum widget
      {
        combobox
      };
      struct range
      {
        std::string_view values[6] = {MTK_SOURCE_NAMES};
        Source init{IncomingChannel};
      };
      halp_meta(description, "Source of the produced channel")
    } channel_source;

    struct : halp::enum_t<Source, "Byte 1 from">
    {
      enum widget
      {
        combobox
      };
      struct range
      {
        std::string_view values[6] = {MTK_SOURCE_NAMES};
        Source init{DataByte1};
      };
      halp_meta(description, "Source of the produced first data byte")
    } byte1_source;

    struct : halp::enum_t<Source, "Byte 2 from">
    {
      enum widget
      {
        combobox
      };
      struct range
      {
        std::string_view values[6] = {MTK_SOURCE_NAMES};
        Source init{DataByte2};
      };
      halp_meta(description, "Source of the produced second data byte")
    } byte2_source;

    struct : halp::spinbox_i32<"Value 1", halp::range{0, 127, 0}>
    {
      halp_meta(description, "Constant usable as a source")
    } arg1;
    struct : halp::spinbox_i32<"Value 2", halp::range{0, 127, 0}>
    {
      halp_meta(description, "Constant usable as a source")
    } arg2;
    struct : halp::spinbox_i32<"Fixed channel", halp::range{1, 16, 1}>
    {
      halp_meta(description, "Channel usable as a source")
    } channel_arg;

    struct : halp::spinbox_i32<"Only channel", halp::range{0, 16, 0}>
    {
      halp_meta(description, "Channel of the messages transformed (0 = all)")
    } channel;
    struct : halp::range_spinbox_t<
                 int, "Byte 1 range",
                 halp::range_slider_range{.min = 0, .max = 127, .init = {0, 127}}>
    {
      halp_meta(
          description, "First data byte range tested, e.g. 0 to 63. The whole range: all.")
    } byte1_range;
    struct : halp::enum_t<Test, "Byte 1 mode">
    {
      halp_meta(description, "Transform messages inside or outside the Byte 1 range")
    } byte1_test;
    struct : halp::range_spinbox_t<
                 int, "Byte 2 range",
                 halp::range_slider_range{.min = 0, .max = 127, .init = {0, 127}}>
    {
      halp_meta(
          description, "Second data byte range tested, e.g. 1 to 127. The whole range: all.")
    } byte2_range;
    struct : halp::enum_t<Test, "Byte 2 mode">
    {
      halp_meta(description, "Transform messages inside or outside the Byte 2 range")
    } byte2_test;

    struct : halp::toggle<"Real-time first">
    {
      halp_meta(
          description,
          "When the original and the produced message are both sent, a real-time "
          "one (clock, start, stop) goes first")
    } realtime_first;
  } inputs;

#undef MTK_KIND_NAMES
#undef MTK_SOURCE_NAMES

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
  } outputs;

  struct ui
  {
    halp_meta(name, "Midi transform")
    halp_meta(layout, halp::layouts::tabs)
    halp_meta(background, halp::colors::background_mid)

    struct
    {
      halp_meta(name, "Convert")
      halp_meta(layout, halp::layouts::hbox)
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::what> what;
        halp::item<&ins::into> into;
      } kinds;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::mode> mode;
        halp::item<&ins::realtime_first> realtime_first;
      } how;
    } convert;

    struct
    {
      halp_meta(name, "Output")
      halp_meta(layout, halp::layouts::hbox)
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::channel_source> channel_source;
        halp::item<&ins::byte1_source> byte1_source;
        halp::item<&ins::byte2_source> byte2_source;
      } sources;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::channel_arg> channel_arg;
        halp::item<&ins::arg1> arg1;
        halp::item<&ins::arg2> arg2;
      } constants;
    } output;

    struct
    {
      halp_meta(name, "Input filter")
      halp_meta(layout, halp::layouts::hbox)
      halp::item<&ins::channel> channel;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::byte1_range> byte1_range;
        halp::item<&ins::byte1_test> byte1_test;
      } byte1;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::byte2_range> byte2_range;
        halp::item<&ins::byte2_test> byte2_test;
      } byte2;
    } filter;
  };

  MidiTransform() noexcept
  {
    inputs.mode.value = Replace;
    inputs.channel_source.value = IncomingChannel;
    inputs.byte1_source.value = DataByte1;
    inputs.byte2_source.value = DataByte2;
  }

  void stop() noexcept { m_stop = true; }

  //! The whole 0-127 range tests nothing
  static bool full(halp::range_slider_value<int> r) noexcept
  {
    return std::min(r.start, r.end) <= 0 && std::max(r.start, r.end) >= 127;
  }
  static bool in_range(halp::range_slider_value<int> r, int v) noexcept
  {
    return v >= std::min(r.start, r.end) && v <= std::max(r.start, r.end);
  }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical) noexcept
  {

    if(m_stop)
    {
      m_stop = false;
      end_everything(0);
    }

    // The messages that would end a produced note may no longer reach it
    const auto what = What(inputs.what.value);
    const auto into = What(inputs.into.value);
    if(what != m_what || into != m_into)
    {
      m_tracker.end_all(0, [this](libremidi::message&& m) { emit(m); });
      m_what = what;
      m_into = into;
    }

    for(const auto& m : inputs.midi)
      process(m);
  }

  static midi_kind to_midi_kind(What w) noexcept
  {
    using enum midi_kind;
    static constexpr midi_kind table[] = {
        note_on,       note_off,    control_change, program_change, channel_pressure,
        poly_pressure, pitch_bend,  time_code,      song_position,  song_select,
        tune_request,  clock,       start,          continue_,      stop,
        active_sensing, reset,      sysex};
    return table[int(w)];
  }

private:
  note_tracker<128, 1> m_tracker;
  //! Note-ons that went out unchanged, by input channel and note.
  std::array<std::array<uint8_t, 128>, 16> m_passed{};
  //! Note-ons that were replaced or dropped, by input channel and note.
  std::array<std::array<uint8_t, 128>, 16> m_removed{};
  What m_what{NoteOn};
  What m_into{NoteOn};
  bool m_stop{};

  void emit(const libremidi::message& m) { outputs.midi.push_back(m); }

  static bool is_realtime(const libremidi::message& m) noexcept
  {
    return m.size() > 0 && m.bytes[0] >= 0xF8;
  }

  static int byte(const libremidi::message& m, int i) noexcept
  {
    return int(m.size()) > i ? m.bytes[i] : 0;
  }

  void end_everything(int64_t ts)
  {
    m_tracker.end_all(ts, [this](libremidi::message&& m) { emit(m); });
    for(int c = 0; c < 16; c++)
    {
      for(int n = 0; n < 128; n++)
      {
        for(int i = 0; i < m_passed[c][n]; i++)
        {
          auto off = libremidi::channel_events::note_off(c + 1, n, 0);
          off.timestamp = ts;
          emit(off);
        }
      }
    }
    m_passed = {};
    m_removed = {};
  }

  bool matches_filter(const libremidi::message& m) const noexcept
  {
    if(is_channel_message(m) && inputs.channel != 0 && channel_of(m) != inputs.channel)
      return false;
    if(!full(inputs.byte1_range.value)
       && in_range(inputs.byte1_range.value, byte(m, 1)) != (inputs.byte1_test == Inside))
      return false;
    if(!full(inputs.byte2_range.value)
       && in_range(inputs.byte2_range.value, byte(m, 2)) != (inputs.byte2_test == Inside))
      return false;
    return true;
  }

  //! Value of a source, in 1-16 when `as_channel`, in 0-127 otherwise.
  int source(Source s, const libremidi::message& m, bool as_channel) const noexcept
  {
    int v{};
    bool is_channel{};
    switch(s)
    {
      case Arg1:
        v = inputs.arg1;
        break;
      case Arg2:
        v = inputs.arg2;
        break;
      case DataByte1:
        v = byte(m, 1);
        break;
      case DataByte2:
        v = byte(m, 2);
        break;
      case ChannelArg:
        v = inputs.channel_arg;
        is_channel = true;
        break;
      case IncomingChannel:
        v = is_channel_message(m) ? channel_of(m) : int(inputs.channel_arg);
        is_channel = true;
        break;
    }
    if(is_channel)
    {
      v = std::clamp(v, 1, 16);
      return as_channel ? v : ((v - 1) * 127 * 2 + 15) / 30;
    }
    v = std::clamp(v, 0, 127);
    return as_channel ? 1 + (v * 15 * 2 + 127) / 254 : v;
  }

  //! Identity of the source of a produced note, see note_tracker::begin.
  static std::pair<int, int> identity(const libremidi::message& m, midi_kind k) noexcept
  {
    using enum midi_kind;
    if(!is_channel_message(m))
      return {128 + int(k), 0};
    const int c = channel_of(m) - 1;
    switch(k)
    {
      case note_on:
      case note_off:
        return {c, byte(m, 1)};
      case control_change:
        return {16 + c, byte(m, 1)};
      case poly_pressure:
        return {32 + c, byte(m, 1)};
      default:
        // program change, pitch bend, channel pressure: 48 to 95
        return {48 + (int(k) - int(program_change)) * 16 + c, 0};
    }
  }

  //! Whether a matching message ends what its source produced.
  bool is_end(const libremidi::message& m, midi_kind k) const noexcept
  {
    using enum midi_kind;
    switch(k)
    {
      case control_change:
      case poly_pressure:
        return byte(m, 2) == 0;
      case channel_pressure:
        return byte(m, 1) == 0;
      default:
        return false;
    }
  }

  void end_produced(const libremidi::message& in, midi_kind k, int velocity)
  {
    const auto [ic, ip] = identity(in, k);
    while(m_tracker.end(ic, ip, in.timestamp, velocity, [this](libremidi::message&& m) {
      emit(m);
    }))
      ;
  }

  //! Sends what a matching message turns into.
  void produce(const libremidi::message& in, midi_kind k)
  {
    const int ch = source(inputs.channel_source, in, true);
    const int b1 = source(inputs.byte1_source, in, false);
    const int b2 = source(inputs.byte2_source, in, false);
    const auto st = [ch](uint8_t s) { return uint8_t(s | (ch - 1)); };
    const auto u = [](int v) { return uint8_t(v); };

    libremidi::message out;
    switch(What(inputs.into.value))
    {
      case NoteOn: {
        end_produced(in, k, 0);
        if(is_end(in, k) || b2 == 0)
          return;
        const auto [ic, ip] = identity(in, k);
        auto emitter = [this](libremidi::message&& m) { emit(m); };
        auto& h = m_tracker.begin(ic, ip, in.timestamp, emitter);
        m_tracker.play(h, ch, b1, b2, in.timestamp, emitter);
        return;
      }
      case NoteOff:
        out = {st(0x80), u(b1), u(b2)};
        break;
      case CC:
        out = {st(0xB0), u(b1), u(b2)};
        break;
      case ProgramChange:
        out = {st(0xC0), u(b1)};
        break;
      case ChannelPressure:
        out = {st(0xD0), u(b1)};
        break;
      case PolyPressure:
        out = {st(0xA0), u(b1), u(b2)};
        break;
      case PitchBend:
        out = {st(0xE0), u(b1), u(b2)};
        break;
      case TimeCode:
        out = {0xF1, u(b1)};
        break;
      case SongPosition:
        out = {0xF2, u(b1), u(b2)};
        break;
      case SongSelect:
        out = {0xF3, u(b1)};
        break;
      case TuneRequest:
        out = {0xF6};
        break;
      case Clock:
        out = {0xF8};
        break;
      case Start:
        out = {0xFA};
        break;
      case Continue:
        out = {0xFB};
        break;
      case Stop:
        out = {0xFC};
        break;
      case ActiveSensing:
        out = {0xFE};
        break;
      case Reset:
        out = {0xFF};
        break;
      case Sysex:
        return;
    }
    out.timestamp = in.timestamp;
    emit(out);
  }

  bool into_is_realtime() const noexcept
  {
    const auto i = What(inputs.into.value);
    return i == Clock || i == Start || i == Continue || i == Stop || i == ActiveSensing
           || i == Reset;
  }

  //! Sends the original and the produced message in the configured order.
  void insert(const libremidi::message& in, midi_kind k, bool before)
  {
    if(inputs.realtime_first)
    {
      if(into_is_realtime() && !is_realtime(in))
        before = true;
      else if(is_realtime(in) && !into_is_realtime())
        before = false;
    }
    if(before)
    {
      produce(in, k);
      emit(in);
    }
    else
    {
      emit(in);
      produce(in, k);
    }
  }

  void process(const libremidi::message& in)
  {
    if(in.size() == 0)
    {
      emit(in);
      return;
    }

    const auto k = classify(in);
    const auto what = What(inputs.what.value);
    const auto mode = Mode(inputs.mode.value);

    if(is_all_notes_off(in))
    {
      m_tracker.end_all(in.timestamp, [this](libremidi::message&& m) { emit(m); });
      const int c = channel_of(in) - 1;
      m_passed[c] = {};
      m_removed[c] = {};
      emit(in);
      return;
    }

    if(k == midi_kind::note_off && in.size() >= 3)
    {
      const int c = channel_of(in) - 1;
      const int n = in.bytes[1];

      // Whether the note-on went out decides the fate of the note-off,
      // whatever What was when it came.
      const bool removed = m_removed[c][n] > 0;
      const bool kept_pair = !removed && m_passed[c][n] > 0;
      if(removed)
        m_removed[c][n]--;
      else if(kept_pair)
        m_passed[c][n]--;

      if(what == NoteOn)
      {
        const bool into_note = What(inputs.into.value) == NoteOn;
        if(removed)
        {
          if(into_note)
            end_produced(in, k, in.bytes[2]);
          return;
        }
        const bool before = mode == InsertBefore;
        if(into_note && before)
          end_produced(in, k, in.bytes[2]);
        emit(in);
        if(into_note && !before)
          end_produced(in, k, in.bytes[2]);
        return;
      }

      if(what != NoteOff || !matches_filter(in))
      {
        if(!removed)
          emit(in);
        return;
      }

      switch(mode)
      {
        case InsertBefore:
        case InsertAfter:
          if(removed)
            produce(in, k);
          else
            insert(in, k, mode == InsertBefore);
          break;
        case Replace:
          if(kept_pair)
            insert(in, k, false);
          else
            produce(in, k);
          break;
        case Drop:
          if(kept_pair)
            emit(in);
          break;
      }
      return;
    }

    const bool match = k == to_midi_kind(what) && matches_filter(in);
    const bool is_on = k == midi_kind::note_on;
    if(!match)
    {
      if(is_on && m_passed[channel_of(in) - 1][in.bytes[1]] < 255)
        m_passed[channel_of(in) - 1][in.bytes[1]]++;
      emit(in);
      return;
    }

    switch(mode)
    {
      case InsertBefore:
      case InsertAfter:
        if(is_on && m_passed[channel_of(in) - 1][in.bytes[1]] < 255)
          m_passed[channel_of(in) - 1][in.bytes[1]]++;
        insert(in, k, mode == InsertBefore);
        break;
      case Replace:
      case Drop:
        if(is_on && m_removed[channel_of(in) - 1][in.bytes[1]] < 255)
          m_removed[channel_of(in) - 1][in.bytes[1]]++;
        if(mode == Replace)
          produce(in, k);
        break;
    }
  }
};
}
