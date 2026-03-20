#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/small_vector.hpp>

#include <array>
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

  struct
  {
    halp::midi_bus<"MIDI in", libremidi::message> midi_in;

    // Sync configuration
    halp::combobox_t<"Sync Mode", SyncMode> sync_mode{
        SyncMode::MTC_With_Clock_Interpolation};

    // MTC configuration
    halp::spinbox_i32<"Offset (seconds)", halp::irange{-128000, 128000, 0}> offset{0};
    halp::combobox_t<"Output Format", OutputFormat> format{OutputFormat::Seconds};
    halp::combobox_t<"Framerate", FrameRate> framerate{FrameRate::Auto};

    // Clock configuration
    halp::spinbox_f32<"Base Tempo", halp::range{20.0, 300.0, 120.0}> base_tempo{120.0};
    halp::knob_f32<"Clock filter", halp::range{0., 1., 0.8}> clock_filter{0.8};
    halp::spinbox_i32<"Song Position", halp::irange{0, 16383, 0}> initial_song_position{
        0};
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
  } outputs;

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
    std::chrono::steady_clock::time_point last_clock_tick;
    ossia::small_pod_vector<double, max_interval_history>
        tick_intervals; // Recent tick intervals for tempo estimation
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
        return 30.0;
      case 0b11:
        return 29.97;
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
        return FrameRate::FPS_30;
      case 0b11:
        return FrameRate::FPS_2997;
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
       msg.bytes[2] == 0x7F &&                     // Device ID (all devices)
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

  void process_clock_interpolation()
  {
    auto now = std::chrono::steady_clock::now();

    // Calculate interval from last clock tick
    if(clock_interp.last_clock_tick.time_since_epoch().count() > 0)
    {
      auto interval
          = std::chrono::duration<double>(now - clock_interp.last_clock_tick).count();

      // Store interval for tempo estimation
      clock_interp.tick_intervals.push_back(interval);

      // Keep only recent history
      while(clock_interp.tick_intervals.size() > clock_interp.max_interval_history)
      {
        clock_interp.tick_intervals.erase(clock_interp.tick_intervals.begin());
      }

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

    clock_interp.last_clock_tick = now;
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

  void operator()()
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
      else if(msg.is_meta_event() && msg.size() >= 10)
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
              process_clock_interpolation();
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
  }
};

} // namespace mtk
