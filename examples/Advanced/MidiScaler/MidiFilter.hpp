#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>

#include <cmath>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/channel_mask.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/midifile_port.hpp>
#include <halp/value_list.hpp>
#include <libremidi/message.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/small_flat_map.hpp>
#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/variant.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace mtk
{
/**
 * @brief A set of message kinds written as text: "note, cc, bend, -sysex".
 *
 * Tokens are separated by commas or spaces, case does not matter. A token
 * prefixed with '-' or '!' removes kinds; a list starting with a removal
 * starts from every kind. Unknown tokens are ignored. Group names: all,
 * channel, system, realtime, transport, note.
 */
struct kind_set
{
  uint32_t mask{};
  bool empty{true};

  bool contains(midi_kind k) const noexcept { return mask & (1u << int(k)); }

  static constexpr uint32_t bit(midi_kind k) noexcept { return 1u << int(k); }

  static uint32_t lookup(std::string_view t) noexcept
  {
    using enum midi_kind;
    constexpr uint32_t all = (1u << midi_kind_count) - 1;
    constexpr uint32_t channel = bit(note_on) | bit(note_off) | bit(control_change)
                                 | bit(program_change) | bit(pitch_bend)
                                 | bit(channel_pressure) | bit(poly_pressure);
    constexpr uint32_t transport = bit(start) | bit(continue_) | bit(stop);
    constexpr uint32_t realtime
        = bit(clock) | transport | bit(active_sensing) | bit(reset);
    struct entry
    {
      std::string_view name;
      uint32_t mask;
    };
    static constexpr entry table[] = {
        {"all", all},
        {"channel", channel},
        {"system", all & ~channel},
        {"realtime", realtime},
        {"transport", transport},
        {"note", bit(note_on) | bit(note_off)},
        {"notes", bit(note_on) | bit(note_off)},
        {"noteon", bit(note_on)},
        {"noteoff", bit(note_off)},
        {"cc", bit(control_change)},
        {"control", bit(control_change)},
        {"program", bit(program_change)},
        {"pc", bit(program_change)},
        {"bend", bit(pitch_bend)},
        {"pb", bit(pitch_bend)},
        {"pressure", bit(channel_pressure)},
        {"aftertouch", bit(channel_pressure)},
        {"at", bit(channel_pressure)},
        {"poly", bit(poly_pressure)},
        {"sysex", bit(sysex)},
        {"mtc", bit(time_code)},
        {"spp", bit(song_position)},
        {"song", bit(song_select)},
        {"tune", bit(tune_request)},
        {"clock", bit(clock)},
        {"start", bit(start)},
        {"continue", bit(continue_)},
        {"stop", bit(stop)},
        {"sensing", bit(active_sensing)},
        {"reset", bit(reset)},
    };
    for(const auto& e : table)
      if(e.name == t)
        return e.mask;
    return 0;
  }

  void parse(std::string_view s) noexcept
  {
    mask = 0;
    empty = true;
    bool first = true;
    std::size_t i = 0;
    while(i < s.size())
    {
      while(i < s.size()
            && (s[i] == ',' || s[i] == ' ' || s[i] == '\t' || s[i] == ';'))
        i++;
      if(i >= s.size())
        break;

      bool remove = false;
      if(s[i] == '-' || s[i] == '!')
      {
        remove = true;
        i++;
      }

      char buf[16]{};
      std::size_t n = 0;
      while(i < s.size() && s[i] != ',' && s[i] != ' ' && s[i] != '\t' && s[i] != ';')
      {
        char c = s[i++];
        if(c >= 'A' && c <= 'Z')
          c = char(c - 'A' + 'a');
        if(c == '_' || c == '-')
          continue;
        if(n < sizeof(buf))
          buf[n] = c;
        n++;
      }
      if(n > sizeof(buf))
        continue;

      const uint32_t k = lookup(std::string_view{buf, n});
      if(k == 0)
        continue;
      if(remove)
      {
        if(first)
          mask = (1u << midi_kind_count) - 1;
        mask &= ~k;
      }
      else
      {
        mask |= k;
      }
      first = false;
      empty = false;
    }
  }
};

//! Keeps a kind_set in sync with a text control without allocating.
struct cached_kind_set
{
  static constexpr std::size_t max_text = 256;
  kind_set set;

  const kind_set& update(std::string_view text) noexcept
  {
    if(text.size() > max_text)
      text = text.substr(0, max_text);
    if(m_parsed && text == std::string_view{m_text.data(), m_size})
      return set;
    std::copy(text.begin(), text.end(), m_text.begin());
    m_size = text.size();
    m_parsed = true;
    set.parse(text);
    return set;
  }

private:
  std::array<char, max_text> m_text{};
  std::size_t m_size{};
  bool m_parsed{};
};

//! A linear map from [in_from, in_to] onto [out_from, out_to].
struct range_map_row
{
  int in_from{};
  int in_to{};
  int out_from{};
  int out_to{};
};

/**
 * @brief The linear maps of a list control: the first row whose input range
 * holds a value maps it. No rows: inactive.
 */
struct range_maps
{
  static constexpr int max_rows = 4;
  std::array<range_map_row, max_rows> rows{};
  int count{};

  bool active() const noexcept { return count > 0; }

  void assign(const auto& list, int min, int max) noexcept
  {
    count = 0;
    for(range_map_row r : list)
    {
      if(count >= max_rows)
        break;
      r.in_from = std::clamp(r.in_from, min, max);
      r.in_to = std::clamp(r.in_to, min, max);
      r.out_from = std::clamp(r.out_from, min, max);
      r.out_to = std::clamp(r.out_to, min, max);
      rows[count++] = r;
    }
  }

  const range_map_row* find(int v) const noexcept
  {
    for(int i = 0; i < count; i++)
    {
      const auto& r = rows[i];
      if(v >= std::min(r.in_from, r.in_to) && v <= std::max(r.in_from, r.in_to))
        return &r;
    }
    return nullptr;
  }

  //! Rounds to the nearest output value.
  static int map(const range_map_row& r, int v) noexcept
  {
    if(r.in_to == r.in_from)
      return r.out_from;
    const double t = double(v - r.in_from) / double(r.in_to - r.in_from);
    return r.out_from + int(std::floor(t * (r.out_to - r.out_from) + 0.5));
  }
};

/**
 * @brief Filters MIDI messages by kind, channel, note, controller and value,
 * remaps channels, notes and controller numbers, and extracts values.
 *
 * Messages that are not kept go to the "Rejected" output, so the object also
 * splits a keyboard or a set of channels.
 *
 * Text lists ("Notes", "CCs", "Value range") are comma-separated inclusive
 * ranges such as "1-4, 6"; notes accept names such as "C-1-B2, C4". An empty
 * list, as every channel on in "Channels", does not restrict anything.
 *
 * A note-off always goes to the output its note-on went to, with the same
 * remapped channel and note, even when the settings changed in between.
 */
struct MidiFilter
{
  halp_meta(name, "Midi filter")
  halp_meta(c_name, "avnd_helpers_midifilter")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-filter.html")
  halp_meta(uuid, "f66d6cf7-693a-47d4-a249-5978cc946e43")
  halp_flag(process_exec);
  halp_meta(category, "Midi/Routing")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Filter MIDI by kind, channel, note, controller and value, remap channels, "
      "notes and controllers, and expose raw or normalized values.")

  enum Mode
  {
    Index,
    Value,
    Both
  };
  enum Type
  {
    CC,
    PitchBend,
    AfterTouch,
    PolyPressure,
    NoteOn,
    NoteOff,
    NoteAny,
    NoteRunning
  };
  enum ValueTest
  {
    Inside,
    Outside
  };
  enum OutOfMap
  {
    Pass,
    Drop
  };

  struct note_int
  {
    int note{};
    int velocity{};
  };
  struct note_float
  {
    int note{};
    float velocity{};
  };

  struct ins
  {
    halp::midi_bus<"MIDI messages", libremidi::message> midi;
    struct : halp::enum_t<Type, "Filter type">
    {
      halp_meta(
          description,
          "Kind of message kept when Kinds is empty, and kind of the value outputs")
    } filter;
    struct : halp::spinbox_i32<"Channel", halp::range{0, 16, 0}>
    {
      halp_meta(description, "Channel (0 = ALL)")
    } channel;
    struct : halp::spinbox_i32<"Index", halp::range{0, 128, 0}>
    {
      halp_meta(description, "Filter (0 = ALL)")
    } index;
    halp::enum_t<Mode, "Mode"> mode;
    halp::toggle<"Note off to zero"> note_off_to_zero;
    halp::toggle<"Zero to note off"> zero_to_note_off;

    struct : halp::lineedit<"Types", "">
    {
      halp_meta(
          description,
          "Message types kept, e.g. note, cc, program, bend, pressure, poly, sysex, mtc, "
          "spp, song, tune, clock, start, continue, stop, sensing, reset, or the "
          "groups all, channel, system, realtime, transport. A leading '-' "
          "removes. Empty: Filter type decides.")
    } kinds;
    struct : halp::channel_mask<"Channels">
    {
      halp_meta(description, "Channels kept")
    } channels;
    struct : halp::lineedit<"Notes", "">
    {
      halp_meta(
          description, "Notes kept, by number or name, e.g. C-1-B2, 60. Empty: all.")
    } notes;
    struct : halp::lineedit<"CCs", "">
    {
      halp_meta(description, "Controller numbers kept, e.g. 1, 7, 64-69. Empty: all.")
    } ccs;
    struct : halp::toggle<"Invert ranges">
    {
      halp_meta(description, "Drop what Channels, Notes and CCs match instead of keeping it")
    } drop_in_range;
    struct : halp::lineedit<"Value range", "">
    {
      halp_meta(
          description,
          "Second data byte tested (velocity, controller value, bend MSB, poly "
          "pressure), e.g. 1-63. Empty: all.")
    } values;
    struct : halp::enum_t<ValueTest, "Value mode">
    {
      halp_meta(description, "Keep values inside or outside the Values list")
    } value_test;
    struct : halp::value_list<"Channel map", range_map_row, range_maps::max_rows>
    {
      halp_meta(
          description,
          "Channels moved from one range onto another, e.g. 1 8 9 16. The first row "
          "whose input range holds a value maps it. No rows: none.")
      static constexpr std::array<halp::list_column, 4> columns()
      {
        return {
            halp::list_column{.name = "In from", .min = 1, .max = 16, .init = 1},
            halp::list_column{.name = "In to", .min = 1, .max = 16, .init = 16},
            halp::list_column{.name = "Out from", .min = 1, .max = 16, .init = 1},
            halp::list_column{.name = "Out to", .min = 1, .max = 16, .init = 16}};
      }
    } channel_map;
    struct : halp::value_list<"Note map", range_map_row, range_maps::max_rows>
    {
      halp_meta(
          description,
          "Notes moved from one range onto another, e.g. 1 8 9 16. The first row "
          "whose input range holds a value maps it. No rows: none.")
      static constexpr std::array<halp::list_column, 4> columns()
      {
        return {
            halp::list_column{.name = "In from", .min = 0, .max = 127, .init = 0},
            halp::list_column{.name = "In to", .min = 0, .max = 127, .init = 127},
            halp::list_column{.name = "Out from", .min = 0, .max = 127, .init = 0},
            halp::list_column{.name = "Out to", .min = 0, .max = 127, .init = 127}};
      }
    } note_map;
    struct : halp::value_list<"CC map", range_map_row, range_maps::max_rows>
    {
      halp_meta(
          description,
          "Controller numbers moved from one range onto another, e.g. 1 8 9 16. The first row "
          "whose input range holds a value maps it. No rows: none.")
      static constexpr std::array<halp::list_column, 4> columns()
      {
        return {
            halp::list_column{.name = "In from", .min = 0, .max = 127, .init = 0},
            halp::list_column{.name = "In to", .min = 0, .max = 127, .init = 127},
            halp::list_column{.name = "Out from", .min = 0, .max = 127, .init = 0},
            halp::list_column{.name = "Out to", .min = 0, .max = 127, .init = 127}};
      }
    } cc_map;
    struct : halp::enum_t<OutOfMap, "Outside map">
    {
      halp_meta(description, "What happens to kept messages outside a map's input range")
    } out_of_map;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI messages", libremidi::message> midi;
    halp::timed_callback<"Raw Output", ossia::variant<int, note_int>> raw;
    halp::timed_callback<"Normalized value", ossia::variant<float, note_float>>
        normalized;

    halp::timed_callback<
        "Raw poly output",
        ossia::variant<
            ossia::small_vector<uint8_t, 128>, ossia::small_vector<note_int, 128>>>
        poly_raw;

    struct : halp::midi_bus<"Rejected", libremidi::message>
    {
      halp_meta(description, "Every message that was not kept")
    } rejected;
  } outputs;

  struct ui
  {
    halp_meta(name, "Midi filter")
    halp_meta(layout, halp::layouts::tabs)
    halp_meta(background, halp::colors::background_mid)

    struct
    {
      halp_meta(name, "Basic")
      halp_meta(layout, halp::layouts::hbox)
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::filter> filter;
        halp::item<&ins::mode> mode;
      } kind;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        struct
        {
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::channel> channel;
          halp::item<&ins::index> index;
        } numbers;
        halp::item<&ins::note_off_to_zero> note_off_to_zero;
        halp::item<&ins::zero_to_note_off> zero_to_note_off;
      } options;
    } basic;

    struct
    {
      halp_meta(name, "Select")
      halp_meta(layout, halp::layouts::hbox)
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::kinds> kinds;
        halp::item<&ins::channels> channels;
        halp::item<&ins::drop_in_range> drop_in_range;
      } left;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::notes> notes;
        halp::item<&ins::ccs> ccs;
        halp::item<&ins::values> values;
        halp::item<&ins::value_test> value_test;
      } right;
    } select;

    struct
    {
      halp_meta(name, "Remap")
      halp_meta(layout, halp::layouts::hbox)
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::channel_map> channel_map;
        halp::item<&ins::cc_map> cc_map;
      } left;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::note_map> note_map;
        halp::item<&ins::out_of_map> out_of_map;
      } right;
    } remap;
  };

  ossia::small_flat_map<int, int, 128> running;
  ossia::small_vector<uint8_t, 128> out_no_vel;
  ossia::small_vector<note_int, 128> out_vel;

  using tick = halp::tick_musical;

  void stop() noexcept { m_stop = true; }

  void operator()() noexcept
  {
    update_settings();

    if(m_stop)
    {
      m_stop = false;
      release_held(0, 0);
      running.clear();
    }

    for(const auto& m : inputs.midi)
      process(m);
  }

private:
  struct held_entry
  {
    bool to_main{}; //!< where the note-on went
    bool base{};    //!< whether it passed the criteria and maps
    bool pass{};    //!< whether it also passed the Index control
    uint8_t channel{}; //!< 1 - 16, after the maps when base is set
    uint8_t note{};    //!< after the maps when base is set
  };
  struct held_slot
  {
    static constexpr int depth = 4;
    std::array<held_entry, depth> entries{};
    uint8_t count{};
  };

  // Settings parsed from the text controls
  cached_kind_set m_kinds;
  cached_range_list m_notes, m_ccs, m_values;
  range_maps m_channel_map, m_note_map, m_cc_map;

  //! Indexed by input (channel - 1) * 128 + note: what each held note-on became.
  std::array<held_slot, 16 * 128> m_held{};
  bool m_stop{};

  void update_settings() noexcept
  {
    m_kinds.update(inputs.kinds.value);
    m_notes.update(inputs.notes.value, 0, 127, true);
    m_ccs.update(inputs.ccs.value, 0, 127);
    m_values.update(inputs.values.value, 0, 127);
    m_channel_map.assign(inputs.channel_map.value, 1, 16);
    m_note_map.assign(inputs.note_map.value, 0, 127);
    m_cc_map.assign(inputs.cc_map.value, 0, 127);
  }

  static bool is_note_kind(midi_kind k) noexcept
  {
    return k == midi_kind::note_on || k == midi_kind::note_off
           || k == midi_kind::poly_pressure;
  }

  void send(bool to_main, const libremidi::message& m)
  {
    if(to_main)
      outputs.midi.push_back(m);
    else
      outputs.rejected.push_back(m);
  }

  void push_held(int channel, int note, held_entry e) noexcept
  {
    auto& slot = m_held[(channel - 1) * 128 + note];
    if(slot.count == held_slot::depth)
    {
      for(int i = 0; i + 1 < held_slot::depth; i++)
        slot.entries[i] = slot.entries[i + 1];
      slot.count--;
    }
    slot.entries[slot.count++] = e;
  }

  bool pop_held(int channel, int note, held_entry& e) noexcept
  {
    auto& slot = m_held[(channel - 1) * 128 + note];
    if(slot.count == 0)
      return false;
    e = slot.entries[0];
    for(int i = 0; i + 1 < slot.count; i++)
      slot.entries[i] = slot.entries[i + 1];
    slot.count--;
    return true;
  }

  //! Sends a note-off for each held note of an input channel (0: all channels).
  void release_held(int channel, int64_t ts)
  {
    for(int c = 1; c <= 16; c++)
    {
      if(channel != 0 && c != channel)
        continue;
      for(int n = 0; n < 128; n++)
      {
        auto& slot = m_held[(c - 1) * 128 + n];
        for(int i = 0; i < slot.count; i++)
        {
          const auto& e = slot.entries[i];
          auto off = libremidi::channel_events::note_off(c, n, 0);
          const bool to_main = off_to_main(e, off);
          if(to_main)
            off = libremidi::channel_events::note_off(e.channel, e.note, 0);
          off.timestamp = ts;
          send(to_main, off);
        }
        slot.count = 0;
      }
    }
  }

  //! A note-off goes where its note-on went, so that a kept note is always
  //! ended. When note-offs are selected, those of notes rejected only for
  //! their kind go to the main output instead, since that is what was asked.
  bool off_to_main(const held_entry& e, const libremidi::message& off) const noexcept
  {
    return e.to_main || (e.pass && kind_accepts(off, midi_kind::note_off));
  }

  //! Kind selection: the Kinds list, or the legacy Filter type when it is empty.
  bool kind_accepts(const libremidi::message& m, midi_kind k) const noexcept
  {
    if(!m_kinds.set.empty)
      return m_kinds.set.contains(k);

    using mt = libremidi::message_type;
    const auto t = m.get_message_type();
    const bool zero = m.size() >= 3 && m.bytes[2] == 0;
    switch(inputs.filter)
    {
      case NoteOn:
        return t == mt::NOTE_ON && !(inputs.zero_to_note_off && zero);
      case NoteOff:
        return t == mt::NOTE_OFF
               || (t == mt::NOTE_ON && inputs.zero_to_note_off && zero);
      case NoteAny:
      case NoteRunning:
        return t == mt::NOTE_ON || t == mt::NOTE_OFF;
      case AfterTouch:
        return t == mt::AFTERTOUCH;
      case CC:
        return t == mt::CONTROL_CHANGE;
      case PolyPressure:
        return t == mt::POLY_PRESSURE;
      case PitchBend:
        return t == mt::PITCH_BEND;
    }
    return false;
  }

  //! The Index control applies to notes, poly pressure and controllers.
  bool index_accepts(const libremidi::message& m, midi_kind k) const noexcept
  {
    const bool keyed = is_note_kind(k) || k == midi_kind::control_change;
    return !keyed || inputs.index == 0 || (m.size() >= 2 && m.bytes[1] == inputs.index);
  }

  //! Channel, range lists and value range. System messages always pass.
  bool criteria(const libremidi::message& m, midi_kind k) const noexcept
  {
    if(!is_channel_message(m))
      return true;

    const int ch = channel_of(m);
    if(inputs.channel != 0 && ch != inputs.channel)
      return false;

    const bool keyed = is_note_kind(k) || k == midi_kind::control_change;
    bool tested = false;
    bool inside = true;
    // Every channel on tests nothing, as an empty list
    if(const uint16_t mask = inputs.channels.mask(); mask != 0xFFFF)
    {
      tested = true;
      inside = inside && ((mask >> (ch - 1)) & 1);
    }
    if(is_note_kind(k) && !m_notes.list.empty() && m.size() >= 2)
    {
      tested = true;
      inside = inside && m_notes.list.contains(m.bytes[1]);
    }
    if(k == midi_kind::control_change && !m_ccs.list.empty() && m.size() >= 2)
    {
      tested = true;
      inside = inside && m_ccs.list.contains(m.bytes[1]);
    }
    if(tested && inside == bool(inputs.drop_in_range))
      return false;

    const bool has_value = keyed || k == midi_kind::pitch_bend;
    if(has_value && !m_values.list.empty() && m.size() >= 3)
    {
      const bool in = m_values.list.contains(m.bytes[2]);
      if(in != (inputs.value_test == Inside))
        return false;
    }
    return true;
  }

  //! Applies the maps. Returns false when the message falls out of a map
  //! that drops.
  bool remap(libremidi::message& m, midi_kind k) const noexcept
  {
    if(!is_channel_message(m))
      return true;
    const bool drop = inputs.out_of_map == Drop;

    if(m_channel_map.active())
    {
      const int ch = channel_of(m);
      if(auto r = m_channel_map.find(ch))
        set_channel(m, range_maps::map(*r, ch));
      else if(drop)
        return false;
    }
    if(is_note_kind(k) && m_note_map.active() && m.size() >= 2)
    {
      if(auto r = m_note_map.find(m.bytes[1]))
        m.bytes[1] = uint8_t(range_maps::map(*r, m.bytes[1]));
      else if(drop)
        return false;
    }
    if(k == midi_kind::control_change && m_cc_map.active() && m.size() >= 2)
    {
      if(auto r = m_cc_map.find(m.bytes[1]))
        m.bytes[1] = uint8_t(range_maps::map(*r, m.bytes[1]));
      else if(drop)
        return false;
    }
    return true;
  }

  void process(const libremidi::message& in)
  {
    if(in.size() == 0)
    {
      outputs.rejected.push_back(in);
      return;
    }

    const auto k = classify(in);
    if(is_all_notes_off(in))
      release_held(channel_of(in), in.timestamp);

    const bool kind_ok = kind_accepts(in, k);

    libremidi::message out = in;
    held_entry tracked;
    const bool is_tracked = k == midi_kind::note_off && in.size() >= 3
                            && pop_held(channel_of(in), in.bytes[1], tracked);

    // base: the criteria and maps, which also gate the value outputs.
    // pass: base and the Index control.
    bool base{}, pass{};
    if(is_tracked)
    {
      base = tracked.base;
      pass = tracked.pass;
      if(base)
      {
        set_channel(out, tracked.channel);
        out.bytes[1] = tracked.note;
      }
    }
    else
    {
      base = criteria(in, k) && remap(out, k);
      if(!base)
        out = in;
      pass = base && index_accepts(in, k);
    }

    const bool to_main = is_tracked ? off_to_main(tracked, in) : (kind_ok && pass);
    if(k == midi_kind::note_on)
    {
      push_held(
          channel_of(in), in.bytes[1],
          {to_main, base, pass, uint8_t(channel_of(out)), out.bytes[1]});
    }

    if(base)
      extract(in, out);

    send(to_main, to_main ? out : in);
  }

  // Value outputs, driven by the Filter type ------------------------------

  void push_poly(int ts)
  {
    out_no_vel.clear();
    out_vel.clear();
    switch(inputs.mode)
    {
      case Both: {
        for(auto [note, vel] : this->running)
          out_vel.push_back({note, vel});
        outputs.poly_raw(ts, out_vel);
        break;
      }
      case Index: {
        for(auto [note, vel] : this->running)
          out_no_vel.push_back(note);
        outputs.poly_raw(ts, out_no_vel);
        break;
      }
      case Value: {
        for(auto [note, vel] : this->running)
          out_no_vel.push_back(vel);
        outputs.poly_raw(ts, out_no_vel);
        break;
      }
    }
  }

  void push_poly_with_note_offs(int ts, int note_off)
  {
    out_no_vel.clear();
    out_vel.clear();
    switch(inputs.mode)
    {
      case Both: {
        for(auto [note, vel] : this->running)
          out_vel.push_back({note, vel});
        out_vel.push_back({note_off, 0});
        outputs.poly_raw(ts, out_vel);
        break;
      }
      case Index: {
        for(auto [note, vel] : this->running)
          out_no_vel.push_back(note);
        out_no_vel.push_back(note_off);
        outputs.poly_raw(ts, out_no_vel);
        break;
      }
      case Value: {
        for(auto [note, vel] : this->running)
          out_no_vel.push_back(note);
        out_no_vel.push_back(note_off);
        outputs.poly_raw(ts, out_no_vel);
        break;
      }
    }
  }

  void output_2_bytes(const libremidi::message& m)
  {
    const int pitch = (int)m.bytes[1];
    const int vel = (int)m.bytes[2];
    switch(inputs.mode)
    {
      case Both: {
        outputs.raw(m.timestamp, note_int{pitch, vel});
        outputs.normalized(m.timestamp, note_float{pitch, vel / 127.f});
        break;
      }
      case Index: {
        outputs.raw(m.timestamp, pitch);
        outputs.normalized(m.timestamp, vel / 127.);
        break;
      }
      case Value: {
        outputs.raw(m.timestamp, vel);
        outputs.normalized(m.timestamp, vel / 127.);
        break;
      }
    }
  }

  //! `in` decides (type, Index), `m` is what is sent out and gives the values.
  void extract(const libremidi::message& in, const libremidi::message& m)
  {
    using mt = libremidi::message_type;
    const auto t = in.get_message_type();
    if(m.size() < 2)
      return;
    const bool index_ok = inputs.index == 0 || inputs.index == in.bytes[1];
    const bool zero = m.size() >= 3 && m.bytes[2] == 0;

    auto add_note_on = [&] {
      running[m.bytes[1]] = m.bytes[2];
      if(index_ok)
        output_2_bytes(m);
      push_poly(m.timestamp);
    };
    auto remove_note_off = [&] {
      running.erase(m.bytes[1]);
      if(index_ok)
        output_2_bytes(m);
      push_poly(m.timestamp);
    };
    auto process_note_on = [&] {
      if(inputs.zero_to_note_off && zero)
        remove_note_off();
      else
        add_note_on();
    };

    // Note-offs only appear in the poly array, and as zero if requested
    auto process_note_off_running = [&] {
      const int pitch = (int)m.bytes[1];
      running.erase(pitch);

      if(index_ok)
      {
        if(inputs.note_off_to_zero)
        {
          switch(inputs.mode)
          {
            case Both: {
              outputs.raw(m.timestamp, note_int{pitch, 0});
              outputs.normalized(m.timestamp, note_float{pitch, 0.f});
              break;
            }
            case Index: {
              outputs.raw(m.timestamp, pitch);
              outputs.normalized(m.timestamp, 0.);
              break;
            }
            case Value: {
              outputs.raw(m.timestamp, 0);
              outputs.normalized(m.timestamp, 0.);
              break;
            }
          }
          push_poly_with_note_offs(m.timestamp, pitch);
        }
        else
        {
          push_poly(m.timestamp);
        }
      }
    };

    switch(inputs.filter)
    {
      case Type::NoteOn:
        if(t == mt::NOTE_ON && m.size() >= 3)
        {
          if(inputs.zero_to_note_off && zero)
          {
            running.erase(m.bytes[1]);
            push_poly(m.timestamp);
          }
          else
          {
            add_note_on();
          }
        }
        break;

      case Type::NoteOff:
        if(m.size() < 3)
          break;
        if(t == mt::NOTE_ON)
        {
          if(inputs.zero_to_note_off && zero)
            remove_note_off();
        }
        else if(t == mt::NOTE_OFF)
          remove_note_off();
        break;

      case Type::NoteAny:
        if(m.size() < 3)
          break;
        if(t == mt::NOTE_ON)
          process_note_on();
        else if(t == mt::NOTE_OFF)
          remove_note_off();
        break;

      case Type::NoteRunning:
        if(m.size() < 3)
          break;
        if(t == mt::NOTE_ON)
        {
          if(inputs.zero_to_note_off && zero)
            process_note_off_running();
          else
            process_note_on();
        }
        else if(t == mt::NOTE_OFF)
          process_note_off_running();
        break;

      case Type::AfterTouch:
        if(t == mt::AFTERTOUCH)
        {
          outputs.raw(m.timestamp, m.bytes[1]);
          outputs.normalized(m.timestamp, m.bytes[1] / 127.);
        }
        break;
      case Type::CC:
        if(t == mt::CONTROL_CHANGE && index_ok && m.size() >= 3)
          output_2_bytes(m);
        break;
      case Type::PolyPressure:
        if(t == mt::POLY_PRESSURE && index_ok && m.size() >= 3)
          output_2_bytes(m);
        break;
      case Type::PitchBend:
        if(t == mt::PITCH_BEND && m.size() >= 3)
        {
          const int pb = m.bytes[2] * 128 + m.bytes[1];
          outputs.raw(m.timestamp, pb);
          outputs.normalized(m.timestamp, pb / (128. * 128.) - 0.5);
        }
        break;
    }
  }
};
}
