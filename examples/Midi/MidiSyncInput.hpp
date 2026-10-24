#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/EventQueue.hpp>
#include <cmath>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/small_vector.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <optional>

namespace mo
{

/**
 * MIDI Time Code Input
 * Extracts timecode from MIDI Time Code (MTC) messages
 * Supports both Full Frame and Quarter Frame messages
 */
struct MIDISyncIn
{
  halp_meta(name, "MIDI Sync In")
  halp_meta(c_name, "midi_sync_input")
  halp_meta(category, "Timing/Midi")
  halp_meta(author, "ossia team")
  halp_meta(description, "Extract timecode from MIDI Time Code messages")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-timecode-input.html")
  halp_meta(uuid, "f8c7e2a3-9b4d-4e5f-a1c2-3d4e5f6a7b8c")
  halp_flag(process_exec);

  enum class FrameRate
  {
    FPS_24 = 0,
    FPS_25 = 1,
    FPS_2997 = 2,
    FPS_30 = 3,
    Auto = 4
  };

  enum class OutputFormat
  {
    Seconds,
    Milliseconds,
    Microseconds,
    Nanoseconds,
    Flicks
  };

  enum class SyncMode
  {
    MTC_Only,                    // Only use MTC, ignore clock
    Clock_Only,                  // Only use clock (derive position from tempo)
    MTC_With_Clock_Interpolation // Use MTC with clock interpolation (default)
  };

  struct ins
  {
    halp::midi_bus<"MIDI in", libremidi::message> midi_in;

    // Sync configuration
    struct : halp::combobox_t<"Sync Mode", SyncMode>
    {
      struct range
      {
        std::string_view values[3]{"MTC only", "Clock only", "MTC and clock"};
        SyncMode init{SyncMode::MTC_With_Clock_Interpolation};
      };
    } sync_mode{{{SyncMode::MTC_With_Clock_Interpolation}}};

    // MTC configuration
    halp::spinbox_i32<"Offset (seconds)", halp::irange{-128000, 128000, 0}> offset{0};
    halp::combobox_t<"Output Format", OutputFormat> format{OutputFormat::Seconds};
    struct : halp::combobox_t<"Framerate", FrameRate>
    {
      struct range
      {
        std::string_view values[5]{"24", "25", "29.97", "30", "Auto"};
        FrameRate init{FrameRate::Auto};
      };
    } framerate{{{FrameRate::Auto}}};

    // Clock configuration
    halp::spinbox_f32<"Base Tempo", halp::range{20.0, 300.0, 120.0}> base_tempo{120.0};
    halp::knob_f32<"Clock filter", halp::range{0., 1., 0.8}> clock_filter{0.8};
    halp::spinbox_i32<"Song Position", halp::irange{0, 16383, 0}> initial_song_position{
        0};

    // Clock out processing
    struct : halp::spinbox_i32<"Clock divide", halp::irange{1, 48, 1}>
    {
      halp_meta(
          description,
          "Clock out passes 1 of every N clocks, counted from each Start or from "
          "the position a Song Position Pointer gives")
    } clock_divide;

    struct : halp::spinbox_i32<"Start shift", halp::irange{-48, 48, 0}>
    {
      halp_meta(
          description,
          "At each Start, Clock out drops this many clocks (positive, later) or "
          "inserts them right after the Start (negative, earlier)")
    } start_shift;

    struct : halp::spinbox_f32<"Clock delay (ms)", halp::range{0., 1000., 0.}>
    {
      halp_meta(description, "Delays everything Clock out sends")
    } clock_delay;

    struct : halp::toggle<"Pass SPP", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "Clock out passes Song Position Pointer messages, counted in the "
          "divided and shifted clock")
    } pass_spp;
  } inputs;

  struct
  {
    // Main outputs
    halp::val_port<"Timecode", double> timecode{0.0};
    halp::val_port<"Tempo", double> estimated_tempo{120.0};
    halp::val_port<"Song Position", int> song_position{0};
    halp::val_port<"Valid", bool> valid{false};
    halp::val_port<"Frame Rate", double> frame_rate{30.0};

    halp::val_port<"MTC Active", bool> mtc_active{false};
    halp::val_port<"Clock Active", bool> clock_active{false};

    struct : halp::midi_bus<"Clock out", libremidi::message>
    {
      halp_meta(
          description,
          "The incoming clock, Start, Stop, Continue and Song Position, divided, "
          "shifted and delayed")
    } clock_out;
  } outputs;

  struct ui
  {
    halp_meta(name, "MIDI Sync In")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Decode")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::sync_mode> sync_mode;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        struct
        {
          halp_meta(layout, halp::layouts::vbox)
          halp::item<&ins::offset> offset;
          halp::item<&ins::format> format;
          halp::item<&ins::framerate> framerate;
        } timecode;
        struct
        {
          halp_meta(layout, halp::layouts::vbox)
          struct
          {
            halp_meta(layout, halp::layouts::vbox)
            static constexpr auto condition()
            {
              return halp::enabled_when<
                  &ins::sync_mode, SyncMode::Clock_Only,
                  SyncMode::MTC_With_Clock_Interpolation>{};
            }
            halp::item<&ins::base_tempo> base_tempo;
            halp::item<&ins::clock_filter> clock_filter;
          } tempo;
          struct
          {
            halp_meta(layout, halp::layouts::vbox)
            static constexpr auto condition()
            {
              return halp::enabled_when<&ins::sync_mode, SyncMode::Clock_Only>{};
            }
            halp::item<&ins::initial_song_position> initial_song_position;
          } song_position;
        } clock;
      } columns;
    } decode;

    struct
    {
      halp_meta(name, "Clock out")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::clock_divide> clock_divide;
      halp::item<&ins::start_shift> start_shift;
      halp::item<&ins::clock_delay> clock_delay;
      halp::item<&ins::pass_spp> pass_spp;
    } clock_out;
  };

  // MTC state tracking
  struct MTCState
  {
    // Current assembled values
    uint8_t hours = 0;
    uint8_t minutes = 0;
    uint8_t seconds = 0;
    uint8_t frames = 0;
    uint8_t frame_rate_bits = 0b10; // Default to 30fps

    // Quarter frame assembly
    std::array<uint8_t, 8> quarter_frames{};
    int quarter_frame_index = 0;
    bool assembling = false;

    // Timing
    std::chrono::steady_clock::time_point last_message_time;
    bool has_valid_time = false;

    // Full frame flag
    bool received_full_frame = false;
  };

  // Clock interpolation state for smooth timecode between MTC updates
  struct ClockInterpolation
  {
    bool active = false;
    double last_mtc_seconds = 0.0; // Last MTC position in seconds
    std::chrono::steady_clock::time_point last_mtc_timestamp;

    // Clock tick tracking
    static constexpr size_t max_interval_history = 24; // 1 beat worth at 24 PPQN
    //! Date of the last clock, in frames since the object started: from the
    //! messages' timestamps, not from when the buffer is processed.
    int64_t last_clock_tick = 0;
    bool has_clock_tick = false;
    //! Recent tick intervals for tempo estimation, never more than
    //! max_interval_history: it does not allocate.
    ossia::small_pod_vector<double, max_interval_history> tick_intervals;
    double estimated_tempo = 120.0;
    double filtered_tempo = 120.0;
    uint32_t ticks_since_mtc = 0;

    // Clock-only mode position tracking
    double clock_position_seconds = 0.0; // Current position when using clock-only
    uint32_t total_tick_count = 0;       // Total ticks since start
    uint16_t song_position_pointer = 0;  // MIDI Song Position (in 16th notes)
    bool transport_running = false;

    void reset()
    {
      active = false;
      ticks_since_mtc = 0;
      tick_intervals.clear();
      has_clock_tick = false;

      estimated_tempo = 120.0;
      filtered_tempo = 120.0;
      clock_position_seconds = 0.0;
      total_tick_count = 0;
      song_position_pointer = 0;
      transport_running = false;
    }

    void start_transport()
    {
      transport_running = true;
      total_tick_count = 0;
      song_position_pointer = 0;
      clock_position_seconds = 0.0;
    }

    void stop_transport() { transport_running = false; }

    void continue_transport() { transport_running = true; }

    void set_song_position(uint16_t position)
    {
      song_position_pointer = position;
      // Convert to ticks: position is in 16th notes, we have 24 PPQN
      // 24 PPQN = 6 ticks per 16th note
      total_tick_count = position * 6;

      // Calculate time position
      double quarters = position / 4.0; // Convert 16th notes to quarter notes
      double seconds_per_quarter = 60.0 / filtered_tempo;
      clock_position_seconds = quarters * seconds_per_quarter;
    }

    double get_interpolated_timecode(double base_mtc_seconds) const
    {
      if(!active || ticks_since_mtc == 0)
        return base_mtc_seconds;

      // Calculate time advance based on clock ticks
      // 24 PPQN means 24 ticks per quarter note
      double quarters_elapsed = ticks_since_mtc / 24.0;
      double seconds_per_quarter = 60.0 / filtered_tempo;
      double time_advance = quarters_elapsed * seconds_per_quarter;

      return base_mtc_seconds + time_advance;
    }

    double get_clock_only_position() const
    {
      // Calculate position from total tick count
      double quarters = total_tick_count / 24.0;
      double seconds_per_quarter = 60.0 / filtered_tempo;
      return quarters * seconds_per_quarter;
    }
  };

  MTCState mtc_state;
  ClockInterpolation clock_interp;

  // Frame rate conversion
  static double get_frame_rate(uint8_t rate_bits)
  {
    switch(rate_bits & 0b11)
    {
      case 0b00:
        return 24.0;
      case 0b01:
        return 25.0;
      case 0b10:
        return 29.97;
      case 0b11:
        return 30.0;
      default:
        return 30.0;
    }
  }

  static FrameRate get_frame_rate_enum(uint8_t rate_bits)
  {
    switch(rate_bits & 0b11)
    {
      case 0b00:
        return FrameRate::FPS_24;
      case 0b01:
        return FrameRate::FPS_25;
      case 0b10:
        return FrameRate::FPS_2997;
      case 0b11:
        return FrameRate::FPS_30;
      default:
        return FrameRate::FPS_30;
    }
  }

  double to_seconds(const MTCState& state) const
  {
    double fps = inputs.framerate == FrameRate::Auto
                     ? get_frame_rate(state.frame_rate_bits)
                     : get_frame_rate(static_cast<uint8_t>(inputs.framerate.value));

    double total_seconds = state.hours * 3600.0 + state.minutes * 60.0 + state.seconds
                           + (state.frames / fps);

    return total_seconds + inputs.offset.value;
  }

  double convert_output(double seconds) const
  {
    switch(inputs.format.value)
    {
      case OutputFormat::Seconds:
        return seconds;
      case OutputFormat::Milliseconds:
        return seconds * 1000.0;
      case OutputFormat::Microseconds:
        return seconds * 1e6;
      case OutputFormat::Nanoseconds:
        return seconds * 1e9;
      case OutputFormat::Flicks:
        return seconds * 705'600'000.0;
      default:
        return seconds;
    }
  }

  void process_quarter_frame(uint8_t data_byte)
  {
    // Extract the message type (upper nibble) and data (lower nibble)
    uint8_t message_type = (data_byte >> 4) & 0x07;
    uint8_t data = data_byte & 0x0F;

    // Store in the appropriate slot
    mtc_state.quarter_frames[message_type] = data;

    // Check if we're starting a new sequence (frame lsb)
    if(message_type == 0)
    {
      mtc_state.assembling = true;
      mtc_state.quarter_frame_index = 0;
    }

    // Update index
    if(mtc_state.assembling)
    {
      mtc_state.quarter_frame_index++;

      // If we've received all 8 quarter frames, assemble the complete time
      if(mtc_state.quarter_frame_index >= 8)
      {
        assemble_time_from_quarter_frames();
        mtc_state.assembling = false;
        mtc_state.quarter_frame_index = 0;
      }
    }
  }

  void assemble_time_from_quarter_frames()
  {
    // Assemble the complete timecode from quarter frames
    // Based on the MTC spec from the MidiSync code

    // Frame number: bits 0 and 1
    mtc_state.frames = (mtc_state.quarter_frames[0] & 0x0F)
                       | ((mtc_state.quarter_frames[1] & 0x01) << 4);

    // Seconds: bits 2 and 3
    mtc_state.seconds = (mtc_state.quarter_frames[2] & 0x0F)
                        | ((mtc_state.quarter_frames[3] & 0x03) << 4);

    // Minutes: bits 4 and 5
    mtc_state.minutes = (mtc_state.quarter_frames[4] & 0x0F)
                        | ((mtc_state.quarter_frames[5] & 0x03) << 4);

    // Hours and frame rate: bits 6 and 7
    mtc_state.hours = (mtc_state.quarter_frames[6] & 0x0F)
                      | ((mtc_state.quarter_frames[7] & 0x01) << 4);

    // Frame rate is in bits 5-6 of the last quarter frame
    mtc_state.frame_rate_bits = (mtc_state.quarter_frames[7] >> 1) & 0x03;

    mtc_state.has_valid_time = true;

    // The last piece arrives two frames after the time the eight of them
    // carry: the position is that much later
    mtc_state.frames += 2;

    // Reset clock interpolation when we get a new MTC frame
    clock_interp.last_mtc_seconds = to_seconds(mtc_state);
    clock_interp.last_mtc_timestamp = std::chrono::steady_clock::now();
    clock_interp.ticks_since_mtc = 0; // Reset tick counter

    update_outputs();
  }

  void process_full_frame(const libremidi::message& msg)
  {
    // Full MTC message format: F0 7F 7F 01 01 hh mm ss ff F7
    if(msg.size() >= 10 && msg.bytes[0] == 0xF0 && // SysEx start
       msg.bytes[1] == 0x7F &&                     // Universal Real Time
       msg.bytes[3] == 0x01 &&                     // MTC
       msg.bytes[4] == 0x01)                       // Full message
    {
      uint8_t h = msg.bytes[5];
      uint8_t m = msg.bytes[6];
      uint8_t s = msg.bytes[7];
      uint8_t f = msg.bytes[8];

      // Extract frame rate from hours byte
      mtc_state.frame_rate_bits = (h >> 5) & 0x03;
      mtc_state.hours = h & 0x1F;
      mtc_state.minutes = m & 0x3F;
      mtc_state.seconds = s & 0x3F;
      mtc_state.frames = f & 0x1F;

      mtc_state.has_valid_time = true;
      mtc_state.received_full_frame = true;

      // A full frame is a locate: quarter frames received before it describe
      // another position, and must not complete a sequence after it.
      mtc_state.quarter_frames = {};
      mtc_state.assembling = false;
      mtc_state.quarter_frame_index = 0;

      // Clocks received before it do not move the new position.
      clock_interp.last_mtc_seconds = to_seconds(mtc_state);
      clock_interp.last_mtc_timestamp = std::chrono::steady_clock::now();
      clock_interp.ticks_since_mtc = 0;

      update_outputs();
    }
  }

  void update_outputs()
  {
    // Always update transport status
    outputs.song_position = clock_interp.song_position_pointer;

    switch(inputs.sync_mode.value)
    {
      case SyncMode::MTC_Only:
        if(mtc_state.has_valid_time)
        {
          double fps
              = inputs.framerate == FrameRate::Auto
                    ? get_frame_rate(mtc_state.frame_rate_bits)
                    : get_frame_rate(static_cast<uint8_t>(inputs.framerate.value));
          outputs.frame_rate = fps;

          double seconds = to_seconds(mtc_state);
          outputs.timecode = convert_output(seconds);

          outputs.valid = true;
          outputs.mtc_active = true;
          outputs.clock_active = false;
        }
        break;

      case SyncMode::Clock_Only:
        if(clock_interp.active)
        {
          double position_seconds = clock_interp.get_clock_only_position();

          double fps
              = inputs.framerate == FrameRate::Auto
                    ? 30.0 // Default for clock-only mode
                    : get_frame_rate(static_cast<uint8_t>(inputs.framerate.value));
          outputs.frame_rate = fps;

          outputs.timecode = convert_output(position_seconds + inputs.offset.value);

          outputs.valid = clock_interp.transport_running;
          outputs.mtc_active = false;
          outputs.clock_active = true;
          outputs.estimated_tempo = clock_interp.filtered_tempo;
        }
        break;

      case SyncMode::MTC_With_Clock_Interpolation:
        if(mtc_state.has_valid_time)
        {
          double fps
              = inputs.framerate == FrameRate::Auto
                    ? get_frame_rate(mtc_state.frame_rate_bits)
                    : get_frame_rate(static_cast<uint8_t>(inputs.framerate.value));
          outputs.frame_rate = fps;

          // Calculate base timecode
          double base_seconds = to_seconds(mtc_state);

          // Apply clock interpolation if available
          double final_seconds = base_seconds;
          if(clock_interp.active && clock_interp.ticks_since_mtc > 0)
          {
            final_seconds = clock_interp.get_interpolated_timecode(base_seconds);
          }

          outputs.timecode = convert_output(final_seconds);

          outputs.clock_active = clock_interp.active;
          outputs.estimated_tempo = clock_interp.filtered_tempo;

          outputs.valid = true;
          outputs.mtc_active = true;
        }
        break;
    }
  }

  //! `at`: the clock's date in frames since the object started.
  void process_clock_interpolation(int64_t at)
  {
    // Calculate interval from last clock tick
    if(clock_interp.has_clock_tick)
    {
      const double rate = sample_rate > 0. ? sample_rate : 48000.;
      const double interval = double(at - clock_interp.last_clock_tick) / rate;

      // Keep only recent history, without growing past the inline storage
      auto& iv = clock_interp.tick_intervals;
      if(iv.size() >= clock_interp.max_interval_history)
        iv.erase(iv.begin());
      iv.push_back(interval);

      // Estimate tempo from tick intervals
      if(clock_interp.tick_intervals.size() >= 2)
      {
        // Calculate average interval
        double sum_intervals = 0.0;
        for(const auto& iv : clock_interp.tick_intervals)
        {
          sum_intervals += iv;
        }
        double avg_interval = sum_intervals / clock_interp.tick_intervals.size();

        // Convert to tempo (24 PPQN)
        if(avg_interval > 0.001) // Sanity check
        {
          double instant_tempo = 60.0 / (avg_interval * 24.0);

          // Apply configurable filtering for stability
          double filter_coeff = inputs.clock_filter.value;
          clock_interp.filtered_tempo = filter_coeff * clock_interp.filtered_tempo
                                        + (1.0 - filter_coeff) * instant_tempo;
          clock_interp.estimated_tempo = clock_interp.filtered_tempo;
        }
      }
    }
    else
    {
      // First tick - initialize tempo from input
      clock_interp.filtered_tempo = inputs.base_tempo.value;
      clock_interp.estimated_tempo = inputs.base_tempo.value;
    }

    clock_interp.last_clock_tick = at;
    clock_interp.has_clock_tick = true;
    clock_interp.active = true;

    // Handle based on sync mode
    switch(inputs.sync_mode.value)
    {
      case SyncMode::MTC_Only:
        // Don't use clock for anything
        break;

      case SyncMode::Clock_Only:
        // Increment position based on clock
        if(clock_interp.transport_running)
        {
          clock_interp.total_tick_count++;
          clock_interp.ticks_since_mtc++; // Still track for debugging
        }
        update_outputs();
        break;

      case SyncMode::MTC_With_Clock_Interpolation:
        // Only interpolate if we have valid MTC
        if(mtc_state.has_valid_time)
        {
          clock_interp.ticks_since_mtc++;
          update_outputs();
        }
        break;
    }
  }

  void check_timeout()
  {
    // Check if we've received MTC recently
    auto now = std::chrono::steady_clock::now();
    if(mtc_state.has_valid_time)
    {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - mtc_state.last_message_time)
                         .count();

      // If no message for more than 500ms, mark as inactive
      if(elapsed > 500)
      {
        outputs.mtc_active = false;
        outputs.valid = false;
      }
    }
  }

  // Clock out state
  static constexpr int max_delayed = 512;
  mtk::event_queue<max_delayed> clock_queue;
  //! Frames elapsed since the object started: the clock of clock_queue.
  int64_t clock_now = 0;
  double sample_rate = 0.;
  int clock_counter = 0;
  int clocks_to_drop = 0;
  std::atomic_bool stop_requested{false};

  void prepare(halp::setup s) noexcept
  {
    if(s.rate > 0.)
      sample_rate = s.rate;
  }

  void stop() noexcept { stop_requested.store(true, std::memory_order_release); }

  void schedule_clock_out(int64_t at, const libremidi::message& msg) noexcept
  {
    clock_queue.schedule(at, msg);
  }

  /**
   * Song Position: the divider and the shift take the phase they have at that
   * position, as if the clock had run from Start, and the position goes out
   * in sixteenths of the clock out, rounded down when it falls between two.
   */
  void song_position_out(int64_t at, const libremidi::message& msg) noexcept
  {
    if(msg.size() < 3)
      return;
    const int n = std::max(1, inputs.clock_divide.value);
    const int64_t spp = int64_t(msg.bytes[1] & 0x7F) | (int64_t(msg.bytes[2] & 0x7F) << 7);
    // Clocks from where the divider starts counting to the position
    const int64_t c = spp * 6 - inputs.start_shift.value;
    clock_counter = c > 0 ? int(c % n) : 0;
    clocks_to_drop = c < 0 ? int(-c) : 0;

    if(!inputs.pass_spp.value)
      return;
    // The clocks out before the position: the ones on k % n == 0, k < c
    const int64_t out_clocks = c > 0 ? (c + n - 1) / n : 0;
    const int64_t out_spp = std::min<int64_t>(out_clocks / 6, 16383);
    libremidi::message m;
    m.bytes = {0xF2, uint8_t(out_spp & 0x7F), uint8_t((out_spp >> 7) & 0x7F)};
    schedule_clock_out(at, m);
  }

  //! A clock that survived Start shift goes through the divider.
  void divide_clock(int64_t at, const libremidi::message& msg) noexcept
  {
    const int n = std::max(1, inputs.clock_divide.value);
    if(clock_counter % n == 0)
      schedule_clock_out(at, msg);
    clock_counter = (clock_counter + 1) % n;
  }

  void process_clock_out(const libremidi::message& msg, int64_t at) noexcept
  {
    if(msg.size() < 1)
      return;

    switch(msg.bytes[0])
    {
      case 0xFA: {
        clock_counter = 0;
        clocks_to_drop = std::max(0, inputs.start_shift.value);
        schedule_clock_out(at, msg);

        libremidi::message clk;
        clk.bytes = {0xF8};
        for(int i = 0; i < -inputs.start_shift.value; i++)
          divide_clock(at, clk);
        break;
      }
      case 0xFB:
      case 0xFC:
        schedule_clock_out(at, msg);
        break;
      case 0xF2:
        song_position_out(at, msg);
        break;
      case 0xF8:
        if(clocks_to_drop > 0)
          clocks_to_drop--;
        else
          divide_clock(at, msg);
        break;
      default:
        break;
    }
  }

  //! On stop the delayed transport messages go out at once and the delayed
  //! clocks are dropped, so that a delayed Stop is not lost.
  void flush_clock_out() noexcept
  {
    clock_queue.remove_if([this](const auto& ev) {
      if(ev.msg.size() > 0 && ev.msg.bytes[0] != 0xF8)
      {
        auto m = ev.msg;
        m.timestamp = 0;
        outputs.clock_out.push_back(std::move(m));
      }
      return true;
    });
    clock_counter = 0;
    clocks_to_drop = 0;
  }

  void run_clock_out(int frames) noexcept
  {
    if(stop_requested.exchange(false, std::memory_order_acq_rel))
      flush_clock_out();

    const int64_t delay
        = sample_rate > 0. ? mtk::seconds_to_frames(
                                 double(inputs.clock_delay.value) / 1000., sample_rate)
                           : 0;
    const int64_t last = frames > 0 ? frames - 1 : 0;
    for(const auto& msg : inputs.midi_in)
      process_clock_out(
          msg, clock_now + std::clamp<int64_t>(msg.timestamp, 0, last) + delay);

    clock_queue.release(clock_now, frames, [this](libremidi::message&& m) {
      outputs.clock_out.push_back(std::move(m));
    });
    if(frames > 0)
      clock_now += frames;
  }

  void operator()(int frames)
  {
    // Process incoming MIDI messages
    for(const auto& msg : inputs.midi_in)
    {
      // Update timestamp
      mtc_state.last_message_time = std::chrono::steady_clock::now();

      // Check for MTC Quarter Frame message (F1)
      if(msg.size() >= 2 && msg.bytes[0] == 0xF1)
      {
        process_quarter_frame(msg.bytes[1]);
      }
      // Check for Full Frame MTC (SysEx)
      else if(msg.size() >= 10 && msg.bytes[0] == 0xF0)
      {
        process_full_frame(msg);
      }
      // Also check for MIDI Clock messages and transport
      // (F8 = clock, FA = start, FB = continue, FC = stop, F2 = song position)
      else if(msg.size() >= 1)
      {
        switch(msg.bytes[0])
        {
          case 0xFA: // Start
            if(inputs.sync_mode.value == SyncMode::Clock_Only)
            {
              // Start from position 0
              clock_interp.start_transport();
              clock_interp.set_song_position(inputs.initial_song_position.value);
            }
            else if(inputs.sync_mode.value == SyncMode::MTC_Only)
            {
              // Reset MTC to 0
              mtc_state.hours = 0;
              mtc_state.minutes = 0;
              mtc_state.seconds = 0;
              mtc_state.frames = 0;
              mtc_state.has_valid_time = true;
            }
            // For MTC_With_Clock_Interpolation, just reset interpolation counter
            clock_interp.ticks_since_mtc = 0;
            update_outputs();
            break;

          case 0xFC: // Stop
            clock_interp.stop_transport();
            update_outputs();
            break;

          case 0xFB: // Continue
            clock_interp.continue_transport();
            update_outputs();
            break;

          case 0xF8: // Clock tick
            // Process clock tick based on mode
            if(inputs.sync_mode.value != SyncMode::MTC_Only)
            {
              process_clock_interpolation(
                  clock_now + std::clamp<int64_t>(msg.timestamp, 0, std::max(frames - 1, 0)));
            }
            break;

          case 0xF2: // Song Position Pointer
            if(msg.size() >= 3 && inputs.sync_mode.value != SyncMode::MTC_Only)
            {
              uint16_t position = (msg.bytes[2] << 7) | msg.bytes[1];
              clock_interp.set_song_position(position);
              update_outputs();
            }
            break;
        }
      }
    }

    // Check for timeout
    check_timeout();

    run_clock_out(frames);
  }
};

} // namespace mtk
