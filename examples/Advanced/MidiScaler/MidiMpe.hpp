#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/ChannelAllocator.hpp>
#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Advanced/MidiScaler/MidiCommon/NoteTracker.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>
#include <rnd/random.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <random>

namespace mtk
{
/**
 * @brief Converts between plain MIDI and MPE (MIDI Polyphonic Expression,
 * lower zone: master channel 1, member channels 2 and up), and shapes the
 * per-note expression: pitch bend, pressure, and slide (CC 74).
 *
 * One object for the three directions: they share the zone, the note
 * tracking and the expression shaping stage, which applies to the output of
 * every mode.
 */
struct MidiMpe
{
  halp_meta(name, "Midi MPE")
  halp_meta(c_name, "midi_mpe")
  halp_meta(category, "Midi/Routing")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Convert MIDI to MPE and back, and shape or randomize per-note pitch bend, "
      "pressure and slide")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-mpe.html")
  halp_meta(uuid, "89a7457e-671e-4d3e-8ab7-45607ee7d0fe")
  halp_flag(process_exec);

  static constexpr int master_channel = 1;
  static constexpr int slide_cc = 74;

  struct mode_control
  {
    static clang_buggy_consteval auto name() { return std::string_view{"Mode"}; }
    halp_meta(
        description,
        "MIDI to MPE: notes get a member channel each. MPE to MIDI: the zone is "
        "collapsed to one channel. MPE to MPE: expression shaping only.")
    enum enum_type
    {
      midi_to_mpe,
      mpe_to_midi,
      mpe_to_mpe
    } value{midi_to_mpe};
    enum widget
    {
      combobox
    };
    struct range
    {
      std::string_view values[3]{"MIDI to MPE", "MPE to MIDI", "MPE to MPE"};
      enum_type init = midi_to_mpe;
    };
    operator enum_type&() noexcept { return value; }
    operator const enum_type&() const noexcept { return value; }
    auto& operator=(enum_type t) noexcept
    {
      value = t;
      return *this;
    }
  };

  struct route_control
  {
    static clang_buggy_consteval auto name() { return std::string_view{"Expression to"}; }
    halp_meta(
        description,
        "MIDI to MPE: channel pitch bend, pressure and CC 74 go to the newest "
        "note or to every held note")
    enum enum_type
    {
      newest,
      all
    } value{newest};
    enum widget
    {
      combobox
    };
    struct range
    {
      std::string_view values[2]{"Newest note", "All notes"};
      enum_type init = newest;
    };
    operator enum_type&() noexcept { return value; }
    operator const enum_type&() const noexcept { return value; }
    auto& operator=(enum_type t) noexcept
    {
      value = t;
      return *this;
    }
  };

  struct ins
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    mode_control mode;

    struct : halp::spinbox_i32<"Members", halp::range{1, 15, 15}>
    {
      halp_meta(
          description,
          "Member channels of the zone: channel 1 is the master channel, members "
          "are channels 2 and up")
    } members;

    struct : halp::spinbox_i32<"Input channel", halp::range{0, 16, 0}>
    {
      halp_meta(description, "MIDI to MPE: channel whose notes are spread (0: all)")
    } input_channel;

    struct : halp::spinbox_i32<"Output channel", halp::range{1, 16, 1}>
    {
      halp_meta(description, "MPE to MIDI: channel the zone is collapsed to")
    } output_channel;

    route_control route;

    struct : halp::toggle<"Send MPE config">
    {
      halp_meta(
          description,
          "MIDI to MPE: send the MPE configuration message and the member pitch "
          "bend range at start and when they change")
    } send_config;

    struct : halp::spinbox_i32<"MIDI bend range", halp::range{1, 96, 2}>
    {
      halp_meta(description, "Pitch bend range in semitones of the plain MIDI side")
    } midi_bend_range;

    struct : halp::spinbox_i32<"MPE bend range", halp::range{1, 96, 48}>
    {
      halp_meta(description, "Pitch bend range in semitones of the member channels")
    } mpe_bend_range;

    struct : halp::hslider_f32<"Release reserve (ms)", halp::range{0., 1000., 0.}>
    {
      halp_meta(
          description,
          "MIDI to MPE: time in ms a member channel stays reserved after its note "
          "ends, for the release of that note")
    } reserve;

    struct : halp::knob_f32<"Pressure curve", halp::range{0.25, 4., 1.}>
    {
      halp_meta(description, "Exponent applied to pressure: above 1 softer, below 1 harder")
    } pressure_curve;

    struct : halp::knob_f32<"Slide curve", halp::range{0.25, 4., 1.}>
    {
      halp_meta(description, "Exponent applied to slide (CC 74)")
    } slide_curve;

    struct : halp::knob_f32<"Bend scale", halp::range{0., 2., 1.}>
    {
      halp_meta(description, "Scales the per-note pitch bend")
    } bend_scale;

    struct : halp::knob_f32<"Random bend", halp::range{0., 2., 0.}>
    {
      halp_meta(description, "Per-note random detune drawn at note start, in semitones")
    } random_bend;

    struct : halp::knob_f32<"Random pressure", halp::range{0., 1., 0.}>
    {
      halp_meta(description, "Per-note random pressure added from note start")
    } random_pressure;

    struct : halp::knob_f32<"Random slide", halp::range{0., 1., 0.}>
    {
      halp_meta(description, "Per-note random slide offset drawn at note start")
    } random_slide;

    struct : halp::spinbox_i32<"Seed", halp::range{0, 10000, 0}>
    {
      halp_meta(description, "0 is free running, other values repeat")
    } seed;
  } inputs;

  struct ui
  {
    halp_meta(name, "Midi MPE")
    halp_meta(layout, halp::layouts::tabs)
    halp_meta(background, halp::colors::background_mid)

    struct
    {
      halp_meta(name, "Zone")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::mode> mode;
        halp::item<&ins::members> members;
      } zone;

      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        struct
        {
          halp_meta(layout, halp::layouts::vbox)
          static constexpr auto condition()
          {
            return halp::enabled_when<&ins::mode, mode_control::midi_to_mpe>{};
          }
          halp::item<&ins::input_channel> input_channel;
        } input;

        struct
        {
          halp_meta(layout, halp::layouts::vbox)
          static constexpr auto condition()
          {
            return halp::enabled_when<&ins::mode, mode_control::mpe_to_midi>{};
          }
          halp::item<&ins::output_channel> output_channel;
        } output;
      } channels;

      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::mode, mode_control::midi_to_mpe>{};
        }
        halp::item<&ins::route> route;
        halp::item<&ins::send_config> send_config;
      } to_mpe;
    } zone;

    struct
    {
      halp_meta(name, "Bend")
      halp_meta(layout, halp::layouts::hbox)
      halp::item<&ins::midi_bend_range> midi_bend_range;
      halp::item<&ins::mpe_bend_range> mpe_bend_range;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::mode, mode_control::midi_to_mpe>{};
        }
        halp::item<&ins::reserve> reserve;
      } reserve;
    } bend;

    struct
    {
      halp_meta(name, "Shaping")
      halp_meta(layout, halp::layouts::hbox)
      halp::item<&ins::pressure_curve> pressure_curve;
      halp::item<&ins::slide_curve> slide_curve;
      halp::item<&ins::bend_scale> bend_scale;
    } shaping;

    struct
    {
      halp_meta(name, "Random")
      halp_meta(layout, halp::layouts::hbox)
      halp::item<&ins::random_bend> random_bend;
      halp::item<&ins::random_pressure> random_pressure;
      halp::item<&ins::random_slide> random_slide;
      halp::item<&ins::seed> seed;
    } random;
  };

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
  } outputs;

  MidiMpe()
  {
    std::random_device rd;
    m_free_seed[0] = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    m_free_seed[1] = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    reseed();
  }

  void prepare(halp::setup info) noexcept
  {
    if(info.rate > 0.)
      m_rate = info.rate;
  }

  //! Sends a message, and frees the channel of each note-off it sends.
  auto emitter() noexcept
  {
    return [this](libremidi::message&& m) noexcept {
      if(is_note_off(m) && m_mode == mode_control::midi_to_mpe)
      {
        const int ch = channel_of(m);
        if(m_taken[ch - 1] > 0)
        {
          m_taken[ch - 1]--;
          m_alloc.release(ch, m_now + m.timestamp);
        }
      }
      outputs.midi.push_back(std::move(m));
    };
  }

  //! Sends a message that is not one of the tracked notes: the member
  //! channels stay as they are.
  void pass(libremidi::message&& m) noexcept { outputs.midi.push_back(std::move(m)); }

  void stop() noexcept { m_stop = true; }

  using tick = halp::tick;
  void operator()(halp::tick t) noexcept
  {
    m_frames = t.frames > 0 ? t.frames : 0;
    auto emit = emitter();

    if(m_stop)
    {
      m_notes.end_all(0, emit);
      m_alloc.reset();
      m_taken = {};
      reset_expression();
      reseed();
      m_stop = false;
    }

    configure(emit);

    for(const auto& in : inputs.midi)
    {
      auto m = in;
      m.timestamp = m_frames > 0 ? std::clamp<int64_t>(in.timestamp, 0, m_frames - 1) : 0;
      switch(m_mode)
      {
        case mode_control::midi_to_mpe:
          handle_midi_to_mpe(m, emit);
          break;
        case mode_control::mpe_to_midi:
          handle_mpe_to_midi(m, emit);
          break;
        case mode_control::mpe_to_mpe:
          handle_mpe_to_mpe(m, emit);
          break;
      }
    }

    m_now += m_frames;
  }

private:
  //! Per-channel expression as received, plus the per-note random offsets.
  struct expression
  {
    double bend{};    //!< semitones
    int pressure{};   //!< 0 - 127
    int slide{64};    //!< 0 - 127
    double rbend{};   //!< semitones
    double rpressure{}; //!< 0 - 127
    double rslide{};  //!< -127 - 127
  };

  //! Last values sent on a channel, -1 when unknown.
  struct sent
  {
    int bend{-1}, pressure{-1}, slide{-1};
  };

  int last_member() const noexcept { return master_channel + m_members; }
  bool in_zone(int ch) const noexcept { return ch >= master_channel && ch <= last_member(); }
  bool is_member(int ch) const noexcept { return ch > master_channel && ch <= last_member(); }

  template <typename Emit>
  void configure(Emit& emit) noexcept
  {
    const auto mode = inputs.mode.value;
    const int members = std::clamp(int(inputs.members.value), 1, 15);
    const int in_ch = std::clamp(int(inputs.input_channel.value), 0, 16);
    const int out_ch = std::clamp(int(inputs.output_channel.value), 1, 16);
    if(mode != m_mode || members != m_members || in_ch != m_input_channel
       || out_ch != m_output_channel)
    {
      // Held notes were placed for the previous settings: their note-offs
      // could no longer be matched.
      m_notes.end_all(0, emit);
      m_mode = mode;
      m_members = members;
      m_input_channel = in_ch;
      m_output_channel = out_ch;
      m_alloc.reset();
      m_taken = {};
      reset_expression();
      m_config_sent = false;
      m_mpe_range = m_mpe_range_control;
      m_param = {};
    }

    uint16_t mask = 0;
    for(int ch = master_channel + 1; ch <= last_member(); ch++)
      mask |= uint16_t(1u << (ch - 1));
    m_alloc.set_channels(mask);
    m_alloc.set_mode(channel_allocator::mode::round_robin);
    m_alloc.set_release_reserve(int64_t(
        std::llround(std::clamp(double(inputs.reserve.value), 0., 1000.) * m_rate / 1000.)));

    m_midi_range = std::clamp(int(inputs.midi_bend_range.value), 1, 96);
    const int mpe_range = std::clamp(int(inputs.mpe_bend_range.value), 1, 96);
    // A range announced by the MPE input (RPN 0 on a member channel) is used
    // until the control changes.
    if(mpe_range != m_mpe_range_control)
    {
      m_mpe_range_control = mpe_range;
      m_mpe_range = mpe_range;
      m_config_sent = false;
    }

    m_pressure_curve = std::clamp(double(inputs.pressure_curve.value), 0.25, 4.);
    m_slide_curve = std::clamp(double(inputs.slide_curve.value), 0.25, 4.);
    m_bend_scale = std::clamp(double(inputs.bend_scale.value), 0., 2.);
    m_random_bend = std::clamp(double(inputs.random_bend.value), 0., 2.);
    m_random_pressure = std::clamp(double(inputs.random_pressure.value), 0., 1.);
    m_random_slide = std::clamp(double(inputs.random_slide.value), 0., 1.);

    if(inputs.seed.value != m_seed)
    {
      m_seed = inputs.seed.value;
      reseed();
    }

    if(!inputs.send_config.value)
      m_config_sent = false;
    else if(!m_config_sent && m_mode == mode_control::midi_to_mpe)
    {
      send_config(emit);
      m_config_sent = true;
    }
  }

  template <typename Emit>
  void send_rpn(int ch, int rpn, int value, Emit& emit) noexcept
  {
    const uint8_t seq[6][2]
        = {{101, 0}, {100, uint8_t(rpn)}, {6, uint8_t(value)}, {38, 0}, {101, 127}, {100, 127}};
    for(auto& cc : seq)
    {
      auto m = libremidi::channel_events::control_change(ch, cc[0], cc[1]);
      m.timestamp = 0;
      emit(std::move(m));
    }
  }

  //! MPE configuration message (RPN 6 on the master channel), then the pitch
  //! bend range of each member (RPN 0), since the former resets the latter
  //! to 48 semitones.
  template <typename Emit>
  void send_config(Emit& emit) noexcept
  {
    send_rpn(master_channel, 6, m_members, emit);
    for(int ch = master_channel + 1; ch <= last_member(); ch++)
      send_rpn(ch, 0, m_mpe_range, emit);
  }

  void reseed() noexcept
  {
    if(m_seed != 0)
      m_rng.seed(uint64_t(m_seed), UINT64_C(0x9e3779b97f4a7c15));
    else
      m_rng.seed(m_free_seed[0], m_free_seed[1]);
  }

  double random01() noexcept { return double(m_rng()) / 4294967296.0; }

  void reset_expression() noexcept
  {
    m_expr = {};
    m_sent = {};
    m_out_sent = {};
    m_in = {};
    m_master_bend = 0.;
  }

  //! New random offsets for the note starting on a channel.
  void draw_offsets(int ch) noexcept
  {
    auto& e = m_expr[ch - 1];
    e.rbend = m_random_bend > 0. ? (random01() * 2. - 1.) * m_random_bend : 0.;
    e.rpressure = m_random_pressure > 0. ? random01() * m_random_pressure * 127. : 0.;
    e.rslide = m_random_slide > 0. ? (random01() * 2. - 1.) * m_random_slide * 127. : 0.;
  }

  bool randomizing() const noexcept
  {
    return m_random_bend > 0. || m_random_pressure > 0. || m_random_slide > 0.;
  }

  static double bend_to_semitones(const libremidi::message& m, int range) noexcept
  {
    if(m.size() < 3)
      return 0.;
    const int v = m.bytes[1] | (m.bytes[2] << 7);
    return double(v - 8192) / 8192. * range;
  }

  static int semitones_to_bend(double semitones, int range) noexcept
  {
    const double v = 8192. + std::round(semitones / double(range) * 8192.);
    return int(std::clamp(v, 0., 16383.));
  }

  static int curve(int v, double exponent) noexcept
  {
    if(exponent == 1.)
      return v;
    return int(std::lround(127. * std::pow(std::clamp(v, 0, 127) / 127., exponent)));
  }

  double shaped_bend(int ch) const noexcept
  {
    const auto& e = m_expr[ch - 1];
    return e.bend * m_bend_scale + e.rbend;
  }

  int shaped_pressure(int ch) const noexcept
  {
    const auto& e = m_expr[ch - 1];
    return std::clamp(
        int(std::lround(curve(e.pressure, m_pressure_curve) + e.rpressure)), 0, 127);
  }

  int shaped_slide(int ch) const noexcept
  {
    const auto& e = m_expr[ch - 1];
    return std::clamp(int(std::lround(curve(e.slide, m_slide_curve) + e.rslide)), 0, 127);
  }

  template <typename Emit>
  void emit_bend(int ch, int value, int64_t ts, Emit& emit) noexcept
  {
    auto m = libremidi::channel_events::pitch_bend(ch, value);
    m.timestamp = ts;
    emit(std::move(m));
  }

  template <typename Emit>
  void emit_pressure(int ch, int value, int64_t ts, Emit& emit) noexcept
  {
    auto m = libremidi::channel_events::aftertouch(ch, value);
    m.timestamp = ts;
    emit(std::move(m));
  }

  template <typename Emit>
  void emit_slide(int ch, int value, int64_t ts, Emit& emit) noexcept
  {
    auto m = libremidi::channel_events::control_change(ch, slide_cc, value);
    m.timestamp = ts;
    emit(std::move(m));
  }

  //! Sends the shaped expression of a member channel, skipping values the
  //! channel already has unless `force`.
  template <typename Emit>
  void send_member(int ch, int64_t ts, bool force, Emit& emit, bool bend = true,
                   bool pressure = true, bool slide = true) noexcept
  {
    auto& s = m_sent[ch - 1];
    if(bend)
    {
      const int v = semitones_to_bend(shaped_bend(ch), m_mpe_range);
      if(force || v != s.bend)
        emit_bend(ch, s.bend = v, ts, emit);
    }
    if(slide)
    {
      const int v = shaped_slide(ch);
      if(force || v != s.slide)
        emit_slide(ch, s.slide = v, ts, emit);
    }
    if(pressure)
    {
      const int v = shaped_pressure(ch);
      if(force || v != s.pressure)
        emit_pressure(ch, s.pressure = v, ts, emit);
    }
  }

  //! Output channel of the newest held note, if any.
  std::optional<int> newest_output() const noexcept
  {
    for(int i = m_notes.size() - 1; i >= 0; i--)
      if(m_notes[i].count > 0)
        return m_notes[i].outputs[0].channel;
    return std::nullopt;
  }

  //! Input channel of the newest held note, if any.
  std::optional<int> newest_input() const noexcept
  {
    if(m_notes.size() == 0)
      return std::nullopt;
    return m_notes[m_notes.size() - 1].channel;
  }

  bool held_on_input(int ch) const noexcept
  {
    for(int i = 0; i < m_notes.size(); i++)
      if(m_notes[i].channel == ch)
        return true;
    return false;
  }

  // ------------------------------------------------------------ MIDI to MPE

  template <typename Emit>
  void handle_midi_to_mpe(libremidi::message& m, Emit& emit) noexcept
  {
    const int64_t ts = m.timestamp;
    const int ch = channel_of(m);
    if(ch == 0 || (m_input_channel != 0 && ch != m_input_channel))
    {
      pass(std::move(m));
      return;
    }

    switch(classify(m))
    {
      case midi_kind::note_on: {
        const int pitch = m.bytes[1];
        auto member = m_alloc.allocate(m_now + ts);
        if(!member)
        {
          // Every member is busy: the one with the fewest notes is shared.
          int best = 0;
          for(int c = master_channel + 1; c <= last_member(); c++)
            if(m_notes.sounding(c, pitch) == 0
               && (best == 0 || m_alloc.held(c) < m_alloc.held(best)))
              best = c;
          if(best == 0)
            break;
          m_alloc.take(best);
          member = best;
        }
        const int c = *member;
        const bool fresh = m_taken[c - 1] == 0;
        m_taken[c - 1]++;
        if(fresh)
          draw_offsets(c);
        m_expr[c - 1].bend = m_in.bend;
        m_expr[c - 1].pressure = m_in.pressure;
        m_expr[c - 1].slide = m_in.slide;
        send_member(c, ts, true, emit);

        auto& h = m_notes.begin(ch, pitch, ts, emit);
        m_notes.play(h, c, pitch, m.bytes[2], ts, emit);
        break;
      }

      case midi_kind::note_off:
        if(!m_notes.end(ch, m.bytes[1], ts, m.bytes[2], emit))
          pass(std::move(m));
        break;

      case midi_kind::pitch_bend:
        m_in.bend = bend_to_semitones(m, m_midi_range);
        route(ts, emit, [&](int c) {
          m_expr[c - 1].bend = m_in.bend;
          send_member(c, ts, false, emit, true, false, false);
        });
        break;

      case midi_kind::channel_pressure:
        if(m.size() < 2)
          break;
        m_in.pressure = m.bytes[1];
        route(ts, emit, [&](int c) {
          m_expr[c - 1].pressure = m_in.pressure;
          send_member(c, ts, false, emit, false, true, false);
        });
        break;

      case midi_kind::poly_pressure: {
        if(m.size() < 3)
          break;
        // Per-note pressure becomes the channel pressure of the note's member.
        for(int i = 0; i < m_notes.size(); i++)
        {
          const auto& h = m_notes[i];
          if(h.channel == ch && h.pitch == m.bytes[1] && h.count > 0)
          {
            const int c = h.outputs[0].channel;
            m_expr[c - 1].pressure = m.bytes[2];
            send_member(c, ts, false, emit, false, true, false);
            break;
          }
        }
        break;
      }

      case midi_kind::control_change:
        if(m.bytes[1] == slide_cc)
        {
          m_in.slide = m.bytes[2];
          route(ts, emit, [&](int c) {
            m_expr[c - 1].slide = m_in.slide;
            send_member(c, ts, false, emit, false, false, true);
          });
        }
        else if(is_all_notes_off(m))
        {
          m_notes.end_all(ts, emit);
          for(int c = master_channel; c <= last_member(); c++)
          {
            auto msg = m;
            set_channel(msg, c);
            emit(std::move(msg));
          }
        }
        else
        {
          set_channel(m, master_channel);
          emit(std::move(m));
        }
        break;

      default:
        set_channel(m, master_channel);
        emit(std::move(m));
        break;
    }
  }

  //! Calls `f` with the member channel of the newest note, or of every held
  //! note, each channel once.
  template <typename Emit, typename F>
  void route(int64_t ts, Emit& emit, F&& f) noexcept
  {
    if(inputs.route.value == route_control::newest)
    {
      if(auto c = newest_output())
        f(*c);
      return;
    }
    for(int c = master_channel + 1; c <= last_member(); c++)
      if(m_taken[c - 1] > 0)
        f(c);
  }

  // ------------------------------------------------------------ MPE to MIDI

  template <typename Emit>
  void handle_mpe_to_midi(libremidi::message& m, Emit& emit) noexcept
  {
    const int64_t ts = m.timestamp;
    const int ch = channel_of(m);
    if(!in_zone(ch))
    {
      emit(std::move(m));
      return;
    }

    switch(classify(m))
    {
      case midi_kind::note_on: {
        if(!held_on_input(ch))
          draw_offsets(ch);
        auto& h = m_notes.begin(ch, m.bytes[1], ts, emit);
        update_collapsed(ts, emit);
        m_notes.play(h, m_output_channel, m.bytes[1], m.bytes[2], ts, emit);
        break;
      }

      case midi_kind::note_off:
        if(m_notes.end(ch, m.bytes[1], ts, m.bytes[2], emit))
          update_collapsed(ts, emit);
        else
        {
          set_channel(m, m_output_channel);
          emit(std::move(m));
        }
        break;

      case midi_kind::pitch_bend:
        if(ch == master_channel)
          m_master_bend = bend_to_semitones(m, m_midi_range);
        else
          m_expr[ch - 1].bend = bend_to_semitones(m, m_mpe_range);
        update_collapsed(ts, emit);
        break;

      case midi_kind::channel_pressure:
        if(m.size() < 2)
          break;
        m_expr[ch - 1].pressure = m.bytes[1];
        update_collapsed(ts, emit);
        break;

      case midi_kind::control_change:
        if(m.bytes[1] == slide_cc && ch != master_channel)
        {
          m_expr[ch - 1].slide = m.bytes[2];
          update_collapsed(ts, emit);
          break;
        }
        if(ch != master_channel && is_parameter_cc(m.bytes[1]))
        {
          // Member channels configure the MPE input; on the single output
          // channel these sequences would retune or reconfigure the receiver.
          read_member_parameter(ch, m.bytes[1], m.bytes[2]);
          break;
        }
        if(is_all_notes_off(m))
          m_notes.end_all(ts, emit);
        set_channel(m, m_output_channel);
        emit(std::move(m));
        break;

      default:
        set_channel(m, m_output_channel);
        emit(std::move(m));
        break;
    }
  }

  //! Bank select excluded: the CCs that select or set an RPN or NRPN.
  static constexpr bool is_parameter_cc(int cc) noexcept
  {
    return cc == 6 || cc == 38 || (cc >= 96 && cc <= 101);
  }

  //! Follows RPN 0 (pitch bend range) on a member channel.
  void read_member_parameter(int ch, int cc, int value) noexcept
  {
    auto& p = m_param[ch - 1];
    switch(cc)
    {
      case 101:
        p.msb = value;
        p.rpn = true;
        break;
      case 100:
        p.lsb = value;
        p.rpn = true;
        break;
      case 99:
        p.msb = value;
        p.rpn = false;
        break;
      case 98:
        p.lsb = value;
        p.rpn = false;
        break;
      case 6:
        if(p.rpn && p.msb == 0 && p.lsb == 0)
          m_mpe_range = std::clamp(value, 1, 96);
        break;
      default:
        break;
    }
  }

  //! Output bend and slide follow the newest note, pressure is the highest
  //! of the held notes.
  template <typename Emit>
  void update_collapsed(int64_t ts, Emit& emit) noexcept
  {
    const auto newest = newest_input();
    if(!newest)
      return;

    const int out = m_output_channel;
    const double semis
        = m_master_bend + (*newest == master_channel ? 0. : shaped_bend(*newest));
    const int bend = semitones_to_bend(semis, m_midi_range);
    if(bend != m_out_sent.bend)
      emit_bend(out, m_out_sent.bend = bend, ts, emit);

    const int slide = shaped_slide(*newest);
    if(slide != m_out_sent.slide)
      emit_slide(out, m_out_sent.slide = slide, ts, emit);

    int pressure = 0;
    for(int i = 0; i < m_notes.size(); i++)
      pressure = std::max(pressure, shaped_pressure(m_notes[i].channel));
    if(pressure != m_out_sent.pressure)
      emit_pressure(out, m_out_sent.pressure = pressure, ts, emit);
  }

  // ------------------------------------------------------------ MPE to MPE

  template <typename Emit>
  void handle_mpe_to_mpe(libremidi::message& m, Emit& emit) noexcept
  {
    const int64_t ts = m.timestamp;
    const int ch = channel_of(m);
    const bool member = is_member(ch);

    switch(classify(m))
    {
      case midi_kind::note_on: {
        if(member && !held_on_input(ch))
        {
          draw_offsets(ch);
          if(randomizing())
            send_member(ch, ts, true, emit);
        }
        auto& h = m_notes.begin(ch, m.bytes[1], ts, emit);
        m_notes.play(h, ch, m.bytes[1], m.bytes[2], ts, emit);
        break;
      }

      case midi_kind::note_off:
        if(!m_notes.end(ch, m.bytes[1], ts, m.bytes[2], emit))
          emit(std::move(m));
        break;

      case midi_kind::pitch_bend:
        if(!member)
        {
          emit(std::move(m));
          break;
        }
        m_expr[ch - 1].bend = bend_to_semitones(m, m_mpe_range);
        send_member(ch, ts, true, emit, true, false, false);
        break;

      case midi_kind::channel_pressure:
        if(!member || m.size() < 2)
        {
          emit(std::move(m));
          break;
        }
        m_expr[ch - 1].pressure = m.bytes[1];
        send_member(ch, ts, true, emit, false, true, false);
        break;

      case midi_kind::control_change:
        if(member && m.bytes[1] == slide_cc)
        {
          m_expr[ch - 1].slide = m.bytes[2];
          send_member(ch, ts, true, emit, false, false, true);
          break;
        }
        if(is_all_notes_off(m))
          m_notes.end_all(ts, emit);
        emit(std::move(m));
        break;

      default:
        emit(std::move(m));
        break;
    }
  }

  note_tracker<128, 1> m_notes;
  channel_allocator m_alloc;
  //! Note-ons sent on each member channel and not yet ended (MIDI to MPE).
  std::array<int, 16> m_taken{};

  std::array<expression, 16> m_expr{};
  std::array<sent, 16> m_sent{};
  sent m_out_sent{};
  //! Current channel-wide expression of the plain MIDI input.
  struct
  {
    double bend{};
    int pressure{};
    int slide{64};
  } m_in;
  double m_master_bend{};

  rnd::pcg m_rng;
  uint64_t m_free_seed[2]{};
  int m_seed{0};

  mode_control::enum_type m_mode{mode_control::midi_to_mpe};
  int m_members{15};
  int m_input_channel{0};
  int m_output_channel{1};
  int m_midi_range{2};
  int m_mpe_range{48};
  int m_mpe_range_control{48};
  //! Parameter selected on each channel of the MPE input; 127/127 is none.
  struct selected_parameter
  {
    int msb{127}, lsb{127};
    bool rpn{true};
  };
  std::array<selected_parameter, 16> m_param{};
  double m_pressure_curve{1.};
  double m_slide_curve{1.};
  double m_bend_scale{1.};
  double m_random_bend{};
  double m_random_pressure{};
  double m_random_slide{};

  double m_rate{48000.};
  int64_t m_now{};
  int m_frames{};
  bool m_config_sent{false};
  bool m_stop{false};
};
}
