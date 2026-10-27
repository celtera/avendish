#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace mo
{
/**
 * Monophonic: one note sounds at a time, chosen among the keys held as a
 * monophonic synthesizer does.
 *
 * - Priority: the held key that sounds, the last one pressed, the lowest or
 *   the highest. A key pressed that does not win is remembered.
 * - On release: when the key of the sounding note is released while others
 *   are held, Stop ends the note and forgets the keys held, whose releases
 *   then do nothing; Return goes back to the held key that wins, at the
 *   velocity it was pressed with.
 * - Transition: from one note to the next, Retrigger ends the first before
 *   starting the second; Legato starts the second first, which monophonic
 *   synthesizers in legato ("fingered") mode take as a slide: glide, and no
 *   new attack.
 * - Latch: Toggle keeps the note sounding when its key is released, until
 *   another key takes over; Momentary does so while Hold is down, and the
 *   note latched ends when Hold goes up.
 *
 * The defaults, Last, Stop, Retrigger and no latch, are a plain monophonic
 * legato. A note is identified by its channel and its pitch: its note-off goes
 * on the channel its note-on went on, and a note-off of the same pitch on
 * another channel leaves it sounding.
 */
struct MidiGlide
{
  halp_meta(name, "Midi Glide")
  halp_meta(c_name, "midi_glide")
  halp_meta(category, "Midi/Pitch");
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(
      description,
      "Monophonic: one note at a time, with the note priority, release, legato "
      "and latch behaviours of monophonic synthesizers")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#midi-glide")
  halp_meta(uuid, "74d26417-1ce5-4765-874e-055921f0a0bb")
  halp_flag(process_exec);

  enum Priority
  {
    Last,
    Low,
    High
  };
  enum Release
  {
    Stop,
    Return
  };
  enum Transition
  {
    Retrigger,
    Legato
  };
  enum Latch
  {
    NoLatch,
    LatchToggle,
    LatchMomentary
  };

  struct ins
  {
    halp::midi_bus<"in", libremidi::message> in;

    struct : halp::enum_t<Priority, "Priority">
    {
      halp_meta(
          description,
          "The held key that sounds: the last one pressed, the lowest or the highest")
    } priority;

    struct : halp::enum_t<Release, "On release">
    {
      halp_meta(
          description,
          "When the sounding key is released while others are held: Stop ends "
          "the note, Return goes back to the held key that wins")
      struct range
      {
        std::string_view values[2] = {"Stop", "Return"};
        Release init = Stop;
      };
    } release;

    struct : halp::enum_t<Transition, "Transition">
    {
      halp_meta(
          description,
          "Retrigger ends a note before starting the next; Legato starts the next "
          "first, so that a monophonic synthesizer glides without a new attack")
    } transition;

    struct : halp::enum_t<Latch, "Latch">
    {
      halp_meta(
          description,
          "Toggle: the note keeps sounding when its key is released, until another "
          "key takes over. Momentary: the same, while Hold is down.")
      enum widget
      {
        combobox
      };
      struct range
      {
        std::string_view values[3] = {"Off", "Toggle", "Momentary"};
        Latch init = NoLatch;
      };
    } latch;

    struct : halp::maintained_button<"Hold">
    {
      halp_meta(
          description,
          "With the Momentary latch: the note keeps sounding while this is down")
    } hold;
  } inputs;

  struct
  {
    halp::midi_bus<"out", libremidi::message> out;
  } outputs;

  struct ui
  {
    halp_meta(name, "Midi Glide")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_mid)
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp::item<&ins::priority> priority;
      halp::item<&ins::release> release;
      halp::item<&ins::transition> transition;
    } mono;
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp::item<&ins::latch> latch;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::latch, int(LatchMomentary)>{};
        }
        halp::item<&ins::hold> hold;
      } hold;
    } latch;
  };

  //! The sounding note ends at the start of the next tick.
  void stop() noexcept { m_stop_requested = true; }

  void operator()() noexcept
  {
    if(m_stop_requested)
    {
      m_stop_requested = false;
      end_current(0, 0);
      m_held_count = 0;
    }

    // Hold going up ends the note it latched, unless its key is down.
    const bool latching = this->latching();
    if(m_latching && !latching && sounding() && !is_held(m_current))
      end_current(0, 0);
    m_latching = latching;

    for(const auto& m : inputs.in)
    {
      if(m.is_note_on_or_off())
      {
        const key k{uint8_t(m.get_channel()), m.bytes[1]};
        const uint8_t vel = m.bytes[2];
        const bool release
            = vel == 0 || m.get_message_type() == libremidi::message_type::NOTE_OFF;
        if(release)
        {
          const bool is_off = m.get_message_type() == libremidi::message_type::NOTE_OFF;
          key_up(k, is_off ? vel : 0, m.timestamp);
        }
        else
        {
          key_down(k, vel, m.timestamp);
        }
      }
      else
      {
        // All sound off and all notes off end the notes at the receiver, and
        // release the keys of the channel.
        if(m.get_message_type() == libremidi::message_type::CONTROL_CHANGE
           && (m.bytes[1] == 120 || m.bytes[1] == 123))
          all_notes_off(m.get_channel());
        outputs.out.push_back(m);
      }
    }
  }

  //! Whether a note sounds at the output.
  bool sounding() const noexcept { return m_current.pitch != no_note; }

private:
  static constexpr uint8_t no_note = 255;
  static constexpr int max_held = 64;

  struct key
  {
    uint8_t channel{1};
    uint8_t pitch{no_note};
    bool operator==(const key&) const noexcept = default;
  };
  struct held_key
  {
    key k;
    uint8_t velocity{};
  };

  bool latching() const noexcept
  {
    return inputs.latch.value == LatchToggle
           || (inputs.latch.value == LatchMomentary && inputs.hold.value);
  }

  bool is_held(key k) const noexcept
  {
    for(int i = 0; i < m_held_count; i++)
      if(m_held[i].k == k)
        return true;
    return false;
  }

  void forget(key k) noexcept
  {
    int j = 0;
    for(int i = 0; i < m_held_count; i++)
      if(!(m_held[i].k == k))
        m_held[j++] = m_held[i];
    m_held_count = j;
  }

  //! The held key that sounds; the latest pressed among equal pitches.
  const held_key* winner() const noexcept
  {
    const held_key* res{};
    for(int i = 0; i < m_held_count; i++)
    {
      const auto& h = m_held[i];
      switch(inputs.priority.value)
      {
        case Last:
          res = &h;
          break;
        case Low:
          if(!res || h.k.pitch <= res->k.pitch)
            res = &h;
          break;
        case High:
          if(!res || h.k.pitch >= res->k.pitch)
            res = &h;
          break;
      }
    }
    return res;
  }

  void send_on(const held_key& h, int64_t ts) noexcept
  {
    auto m = libremidi::channel_events::note_on(h.k.channel, h.k.pitch, h.velocity);
    m.timestamp = ts;
    outputs.out.push_back(m);
  }

  //! The sounding note goes to `next`, ended first or after as Transition says.
  void move_to(const held_key& next, int64_t ts) noexcept
  {
    // The same note cannot overlap itself at the receiver
    if(!sounding() || inputs.transition.value == Retrigger || m_current == next.k)
    {
      end_current(0, ts);
      send_on(next, ts);
    }
    else
    {
      send_on(next, ts);
      end_current(0, ts);
    }
    m_current = next.k;
  }

  void key_down(key k, uint8_t velocity, int64_t ts) noexcept
  {
    forget(k);
    if(m_held_count == max_held)
    {
      for(int i = 0; i + 1 < max_held; i++)
        m_held[i] = m_held[i + 1];
      m_held_count--;
    }
    m_held[m_held_count++] = {k, velocity};

    const held_key* w = winner();
    if(w && (w->k == k || !sounding()))
      move_to(*w, ts);
  }

  void key_up(key k, uint8_t velocity, int64_t ts) noexcept
  {
    forget(k);
    if(!sounding() || !(m_current == k))
      return;

    if(inputs.release.value == Return)
    {
      if(const held_key* w = winner())
      {
        move_to(*w, ts);
        return;
      }
    }
    // Stop: the keys still down do not come back.
    m_held_count = 0;
    if(!latching())
      end_current(velocity, ts);
  }

  void all_notes_off(int channel) noexcept
  {
    int j = 0;
    for(int i = 0; i < m_held_count; i++)
      if(m_held[i].k.channel != channel)
        m_held[j++] = m_held[i];
    m_held_count = j;
    if(m_current.channel == channel)
      m_current.pitch = no_note;
  }

  void end_current(uint8_t velocity, int64_t ts) noexcept
  {
    if(!sounding())
      return;
    outputs.out.push_back(
        libremidi::channel_events::note_off(m_current.channel, m_current.pitch, velocity));
    outputs.out.back().timestamp = ts;
    m_current.pitch = no_note;
  }

  std::array<held_key, max_held> m_held{};
  int m_held_count{};
  key m_current{};
  bool m_latching{};
  bool m_stop_requested{};
};
}
