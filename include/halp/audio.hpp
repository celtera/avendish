#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/span_polyfill.hpp>
#include <boost/container/small_vector.hpp>
#include <cmath>
#include <halp/inline.hpp>
#include <halp/modules.hpp>
#include <halp/static_string.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
HALP_MODULE_EXPORT
namespace halp
{
template <static_string Name, typename FP, static_string Desc = "">
struct audio_sample
{
  static consteval auto name() { return std::string_view{Name.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }

  FP sample;

  HALP_INLINE_FLATTEN operator FP&() noexcept { return sample; }
  HALP_INLINE_FLATTEN operator FP() const noexcept { return sample; }
  HALP_INLINE_FLATTEN auto& operator=(FP t) noexcept
  {
    sample = t;
    return *this;
  }
};

template <static_string Name, typename FP, static_string Desc = "">
struct audio_channel
{
  static consteval auto name() { return std::string_view{Name.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }

  FP* channel;

  HALP_INLINE_FLATTEN operator FP*() const noexcept { return channel; }
  HALP_INLINE_FLATTEN FP& operator[](int i) noexcept { return channel[i]; }
  HALP_INLINE_FLATTEN FP operator[](int i) const noexcept { return channel[i]; }
};

template <static_string lit, typename FP, int WantedChannels, static_string Desc = "">
struct fixed_audio_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }
  static constexpr int channels() { return WantedChannels; }

  FP** samples{};
  HALP_INLINE_FLATTEN operator FP**() const noexcept { return samples; }
  HALP_INLINE_FLATTEN auto get() const noexcept { return *this; }
  HALP_INLINE_FLATTEN FP* operator[](int i) const noexcept { return samples[i]; }
  HALP_INLINE_FLATTEN avnd::span<FP>
  channel(std::size_t i, std::size_t frames) const noexcept
  {
    if(i < WantedChannels)
      return {samples[i], frames};
    else
      return {};
  }
};

template <typename FP>
struct dynamic_audio_bus_base
{
  FP** samples{};
  int channels{};

  HALP_INLINE_FLATTEN operator FP**() const noexcept { return samples; }
  HALP_INLINE_FLATTEN auto get() const noexcept { return *this; }
  HALP_INLINE_FLATTEN FP* operator[](int i) const noexcept { return samples[i]; }
  HALP_INLINE_FLATTEN avnd::span<FP>
  channel(std::size_t i, std::size_t frames) const noexcept
  {
    if(i < channels)
      return {samples[i], frames};
    else
      return {};
  }

  FP** begin() noexcept { return samples; }
  FP** end() noexcept { return samples + channels; }
  FP* const* begin() const noexcept { return samples; }
  FP* const* end() const noexcept { return samples + channels; }
  FP* const* cbegin() noexcept { return samples; }
  FP* const* cend() noexcept { return samples + channels; }
  FP* const* cbegin() const noexcept { return samples; }
  FP* const* cend() const noexcept { return samples + channels; }
};

template <static_string Name, typename FP, static_string Desc = "">
struct dynamic_audio_bus : dynamic_audio_bus_base<FP>
{
  static consteval auto name() { return std::string_view{Name.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }
};

template <static_string Name, typename FP, static_string Desc = "">
struct audio_spectrum_channel
{
  static consteval auto name() { return std::string_view{Name.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }

  FP* channel{};

  struct
  {
    enum window
    {
      hanning
    };
    FP* amplitude{};
    FP* phase{};
  } spectrum;

  HALP_INLINE_FLATTEN operator FP*() const noexcept { return channel; }
  HALP_INLINE_FLATTEN FP& operator[](int i) noexcept { return channel[i]; }
  HALP_INLINE_FLATTEN FP operator[](int i) const noexcept { return channel[i]; }
};

template <static_string lit, typename FP, int WantedChannels, static_string Desc = "">
struct fixed_audio_spectrum_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }
  static constexpr int channels() { return WantedChannels; }

  FP** samples{};

  struct
  {
    enum window
    {
      hanning
    };
    FP** amplitude{};
    FP** phase{};
  } spectrum;

  HALP_INLINE_FLATTEN operator FP**() const noexcept { return samples; }
  HALP_INLINE_FLATTEN FP* operator[](std::size_t i) const noexcept { return samples[i]; }
  HALP_INLINE_FLATTEN avnd::span<FP>
  channel(std::size_t i, std::size_t frames) const noexcept
  {
    if(i < WantedChannels)
      return {samples[i], frames};
    else
      return {};
  }
};

template <static_string Name, typename FP, static_string Desc = "">
struct dynamic_audio_spectrum_bus
{
  static consteval auto name() { return std::string_view{Name.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }

  FP** samples{};
  struct
  {
    enum window
    {
      hanning
    };
    FP** amplitude{};
    FP** phase{};
  } spectrum;

  int channels{};

  HALP_INLINE_FLATTEN operator FP**() const noexcept { return samples; }
  HALP_INLINE_FLATTEN auto get() const noexcept { return *this; }
  HALP_INLINE_FLATTEN FP* operator[](std::size_t i) const noexcept { return samples[i]; }
  HALP_INLINE_FLATTEN avnd::span<FP>
  channel(std::size_t i, std::size_t frames) const noexcept
  {
    if(i < channels)
      return {samples[i], frames};
    else
      return {};
  }
};

template <halp::static_string Name, typename FP, halp::static_string Desc = "">
struct variable_audio_bus : halp::dynamic_audio_bus<Name, FP, Desc>
{
  std::function<void(int)> request_channels;
};

using quantification_frames = boost::container::small_vector<std::pair<int, int>, 8>;
using quarter_note = double;
struct tick
{
  int frames{};
};

struct tick_musical
{
  // Number of audio frames in the current call
  int frames{};

  double tempo = 120.;
  struct
  {
    int num{};
    int denom{};
  } signature;

  // Number of audio frames elapsed since playback started
  int64_t position_in_frames{};

  // Real time as reported by the soundcard or operating system
  double position_in_nanoseconds{};

  // Quarter note of the first sample in the buffer
  quarter_note start_position_in_quarters{};

  // Quarter note of the first sample in the next buffer
  // (or one past the last sample of this buffer, e.g. a [closed; open) interval like C++ begin / end)
  quarter_note end_position_in_quarters{};

  // Position of the last signature change in quarter notes (at the start of the tick)
  quarter_note last_signature_change{};

  // Position of the last bar relative to start in quarter notes
  quarter_note bar_at_start{};

  // Position of the last bar relative to end in quarter notes
  quarter_note bar_at_end{};

  // Position in bar to frames
  constexpr auto pos_to_frame(double in_bar) const noexcept
  {
    double start = start_position_in_quarters;
    double musical_pos = in_bar + bar_at_start;
    double end = end_position_in_quarters;

    double percent = (musical_pos - start) / (end - start);
    int f = percent * this->frames;
    return f;
  }
  constexpr int64_t prev_frame() const noexcept { return position_in_frames; }
  constexpr int64_t end_frame() const noexcept { return position_in_frames + frames; }
  //! Given a quantification rate (1 for bars, 2 for half, 4 for quarters...)
  //! return all occurring quantification dates in the tick
  [[nodiscard]] quantification_frames get_quantification_date(double rate) const noexcept
  {
    quantification_frames result;

    if(prev_frame() == end_frame())
      return result;

    if(rate <= 0.)
    {
      result.emplace_back(0, 0);
      return result;
    }

    const double musical_tick_duration
        = end_position_in_quarters - start_position_in_quarters;
    if(musical_tick_duration <= 0.)
    {
      result.emplace_back(0, 0);
      return result;
    }

    if(rate <= 1.)
    {
      // Bars or longer - use logic from get_quantification_date_for_bars_or_longer
      const double bars_per_quantization = 1.0 / rate;

      // Convert positions to bar numbers from the last signature
      const double start_bar_position
          = (start_position_in_quarters - last_signature_change)
            / (4.0 * signature.num / signature.denom);
      const double end_bar_position = (end_position_in_quarters - last_signature_change)
                                      / (4.0 * signature.num / signature.denom);

      // Check if we're exactly on a quantization point at the start
      const double start_remainder
          = std::fmod(start_bar_position, bars_per_quantization);
      if(std::abs(start_remainder) < 0.0001 && start_position_in_quarters >= 0)
      {
        result.emplace_back(
            0, static_cast<int>(std::round(start_bar_position / bars_per_quantization)));
      }

      // Find all quantization points after start and before end
      const double start_quant_bar
          = std::floor(start_bar_position / bars_per_quantization);
      double next_quant_bar_number = (start_quant_bar + 1) * bars_per_quantization;

      while(next_quant_bar_number < end_bar_position)
      {
        // Calculate the musical position of this quantization point
        const double quant_musical_position
            = last_signature_change
              + next_quant_bar_number * (4.0 * signature.num / signature.denom);

        // Map this to a time value
        const double ratio = (quant_musical_position - start_position_in_quarters)
                             / musical_tick_duration;
        const int64_t dt = end_frame() - prev_frame();
        int64_t frame_offset = dt * ratio;

        if(frame_offset < dt)
        {
          result.emplace_back(
              frame_offset, static_cast<int>(std::round(
                                next_quant_bar_number / bars_per_quantization)));
        }

        next_quant_bar_number += bars_per_quantization;
      }
    }
    else
    {
      // Shorter than bars - subdivisions of quarters

      // Special handling when bar boundary occurs within this tick
      if(bar_at_start != bar_at_end)
      {
        // There's a bar boundary within this tick
        // We need to check for quantifications both before and after the boundary

        // Check if there's a quantification exactly at the bar boundary
        const double bar_boundary_position
            = bar_at_end; // This is the musical position of the bar boundary

        // Calculate if this bar boundary position is a subdivision point
        const double subdivisions_per_quarter = rate / 4.;

        // Check if the bar boundary aligns with our subdivision rate
        // For quarters (rate=4), every quarter boundary is a quantification point
        // For eighths (rate=8), every eighth boundary is a quantification point, etc.
        const double bar_relative_position
            = 0.0; // At a bar boundary, we're at position 0 within the new bar
        const double subdivision_at_boundary
            = bar_relative_position * subdivisions_per_quarter;

        // If the bar boundary is within our tick range, it's always index 0
        if(bar_boundary_position >= start_position_in_quarters
           && bar_boundary_position <= end_position_in_quarters
           && std::abs(subdivision_at_boundary - std::round(subdivision_at_boundary))
                  < 0.0001)
        {
          // Calculate the frame position of this bar boundary
          const double offset_in_quarters
              = bar_boundary_position - start_position_in_quarters;
          const double ratio = offset_in_quarters / musical_tick_duration;
          int64_t frame_offset = std::round(ratio * frames);

          if(frame_offset >= 0 && frame_offset < frames)
          {
            result.emplace_back(frame_offset, 0); // Bar boundaries are always index 0
          }
          else if(frame_offset == frames)
          {
            // Bar boundary is exactly at the end of the tick - use the last frame
            result.emplace_back(frames - 1, 0); // Bar boundaries are always index 0
          }
        }
      }

      const double start_quarter = start_position_in_quarters - bar_at_start;
      const double end_quarter = end_position_in_quarters - bar_at_start;

      // How many subdivisions per quarter note
      // rate = 4 -> 1 per quarter, rate = 8 -> 2 per quarter, rate = 16 -> 4 per quarter
      const double subdivisions_per_quarter = rate / 4.;

      // Calculate actual number of subdivisions that can occur in this time signature
      // For example: 7/8 time has 7 eighth notes, so 4 quarter note positions (0,2,4,6)
      const int base_units_per_bar = signature.num; // e.g., 7 eighth notes in 7/8
      const int base_unit_denom = signature.denom;  // e.g., 8 (eighth notes)

      // How many subdivisions actually fit in the bar
      int subdivisions_per_bar;
      if(rate >= base_unit_denom)
      {
        // Subdivision is smaller than or equal to base unit (e.g., 16th notes in 7/8)
        subdivisions_per_bar
            = base_units_per_bar * (static_cast<int>(rate) / base_unit_denom);
      }
      else
      {
        // Subdivision is larger than base unit (e.g., quarter notes in 7/8)
        // Quarter notes occur every 2 eighth notes, so positions: 0, 2, 4, 6...
        const int subdivision_interval = base_unit_denom / static_cast<int>(rate);
        subdivisions_per_bar = (base_units_per_bar + subdivision_interval - 1)
                               / subdivision_interval; // Ceiling division
      }

      // Find first subdivision at or after start
      const double start_subdivision = start_quarter * subdivisions_per_quarter;
      int current_subdivision_index = static_cast<int>(std::floor(start_subdivision));

      // Check if we start exactly on a subdivision
      if(std::abs(start_subdivision - current_subdivision_index) < 0.0001)
      {
        // Calculate position within the current bar
        const double absolute_quarter_pos
            = start_position_in_quarters - last_signature_change;
        const double quarters_per_bar_exact = 4.0 * signature.num / signature.denom;
        const double position_in_current_bar
            = std::fmod(absolute_quarter_pos, quarters_per_bar_exact);

        // Convert to subdivision position within bar and get index
        const double subdivisions_in_current_bar
            = position_in_current_bar * subdivisions_per_quarter;

        // Find which subdivision slot this represents within the bar's subdivision pattern
        int metric_index;
        if(rate >= base_unit_denom)
        {
          // For subdivisions smaller than the base unit (e.g., 16ths in 7/8)
          metric_index = static_cast<int>(std::round(subdivisions_in_current_bar))
                         % subdivisions_per_bar;
        }
        else
        {
          // For subdivisions larger than base unit (e.g., quarters in 7/8)
          // Count how many of this subdivision type have occurred within the bar
          const int subdivision_interval = base_unit_denom / static_cast<int>(rate);
          const double subdivision_position_in_bar
              = position_in_current_bar * (base_unit_denom / 4.0);
          metric_index = static_cast<int>(std::floor(
                             subdivision_position_in_bar / subdivision_interval))
                         % subdivisions_per_bar;
        }

        result.emplace_back(0, metric_index);
        current_subdivision_index++;
      }
      else
      {
        current_subdivision_index++;
      }

      // Find all subdivisions in the tick
      const double end_subdivision = end_quarter * subdivisions_per_quarter;

      while(current_subdivision_index < end_subdivision)
      {
        // Calculate the position in quarters for this subdivision
        const double quarter_position
            = current_subdivision_index / subdivisions_per_quarter;

        // Calculate the absolute musical position
        const double absolute_musical_position = bar_at_start + quarter_position;

        // Calculate frame offset within this tick
        const double offset_in_quarters
            = absolute_musical_position - start_position_in_quarters;
        const double ratio = offset_in_quarters / musical_tick_duration;
        int64_t frame_offset = std::round(ratio * frames);

        if(frame_offset < frames)
        {
          // Calculate position within the current bar
          const double absolute_quarter_pos
              = absolute_musical_position - last_signature_change;
          const double quarters_per_bar_exact = 4.0 * signature.num / signature.denom;
          const double position_in_current_bar
              = std::fmod(absolute_quarter_pos, quarters_per_bar_exact);

          // Convert to subdivision position within bar and get index
          const double subdivisions_in_current_bar
              = position_in_current_bar * subdivisions_per_quarter;

          // Find which subdivision slot this represents within the bar's subdivision pattern
          int metric_index;
          if(rate >= base_unit_denom)
          {
            // For subdivisions smaller than the base unit (e.g., 16ths in 7/8)
            metric_index = static_cast<int>(std::round(subdivisions_in_current_bar))
                           % subdivisions_per_bar;
          }
          else
          {
            // For subdivisions larger than base unit (e.g., quarters in 7/8)
            // Count how many of this subdivision type have occurred within the bar
            const int subdivision_interval = base_unit_denom / static_cast<int>(rate);
            const double subdivision_position_in_bar
                = position_in_current_bar * (base_unit_denom / 4.0);
            metric_index = static_cast<int>(std::floor(
                               subdivision_position_in_bar / subdivision_interval))
                           % subdivisions_per_bar;
          }

          result.emplace_back(frame_offset, metric_index);
        }

        current_subdivision_index++;
      }
    }

    return result;
  }

  [[nodiscard]] std::optional<int64_t>
  get_quantification_date_for_bars_or_longer(double rate) const noexcept
  {
    std::optional<int64_t> quantification_date;
    const double bars_per_quantization = 1.0 / rate;

    // Convert positions to bar numbers from the last signature
    const double start_bar_position
        = (start_position_in_quarters - last_signature_change)
          / (4.0 * signature.num / signature.denom);
    const double end_bar_position = (end_position_in_quarters - last_signature_change)
                                    / (4.0 * signature.num / signature.denom);

    // Check if we're exactly on a quantization point at the start
    const double start_remainder = std::fmod(start_bar_position, bars_per_quantization);
    if(std::abs(start_remainder) < 0.0001 && start_position_in_quarters >= 0)
    {
      quantification_date = prev_frame();
    }
    else
    {
      // Find the next quantization bar after start
      const double start_quant_bar
          = std::floor(start_bar_position / bars_per_quantization);
      const double next_quant_bar_number = (start_quant_bar + 1) * bars_per_quantization;

      // Check if this quantization point falls within our tick (but NOT at the end)
      if(next_quant_bar_number > start_bar_position
         && next_quant_bar_number < end_bar_position)
      {
        // Calculate the musical position of this quantization point
        const double quant_musical_position
            = last_signature_change
              + next_quant_bar_number * (4.0 * signature.num / signature.denom);

        // Map this to a time value
        const double musical_tick_duration
            = end_position_in_quarters - start_position_in_quarters;
        const double ratio = (quant_musical_position - start_position_in_quarters)
                             / musical_tick_duration;
        const int64_t dt = end_frame() - prev_frame();

        int64_t potential_date = prev_frame() + dt * ratio;

        // Extra safety check: ensure we're not at the boundary
        if(potential_date < end_frame())
        {
          quantification_date = potential_date;
        }
      }
    }
    return quantification_date;
  }

  [[nodiscard]] std::optional<int64_t>
  get_quantification_date_for_shorter_than_bars(double rate) const noexcept
  {
    std::optional<int64_t> quantification_date;
    // Quantize relative to quarter divisions
    // TODO ! if there is a bar change,
    // and no prior quantization date before that, we have to quantize to the
    // bar change
    const double start_quarter = (start_position_in_quarters - bar_at_start);
    const double end_quarter = (end_position_in_quarters - bar_at_start);

    // duration of what we quantify in terms of quarters
    const double musical_quant_dur = rate / 4.;
    const double start_quant = std::floor(start_quarter * musical_quant_dur);
    const double end_quant = std::floor(end_quarter * musical_quant_dur);

    if(start_quant != end_quant)
    {
      // We want quantization on start, not on end
      if(end_quant != end_quarter * musical_quant_dur)
      {
        // Date to quantify is the next one :
        const double musical_tick_duration
            = end_position_in_quarters - start_position_in_quarters;
        const double quantified_duration = (bar_at_start + (start_quant + 1) * 4. / rate)
                                           - start_position_in_quarters;
        const double ratio = (end_frame() - prev_frame()) / musical_tick_duration;

        quantification_date = prev_frame() + quantified_duration * ratio;
      }
    }
    else if(start_quant == start_quarter * musical_quant_dur)
    {
      // We start on a signature change
      quantification_date = prev_frame();
    }
    return quantification_date;
  }

  //! Given a quantification rate (1 for bars, 2 for half, 4 for quarters...)
  //! return the next occurring quantification date, if such date is in the tick
  //! defined by this token_request.
  [[nodiscard]] std::optional<int64_t>
  get_one_quantification_date(double rate) const noexcept
  {
    if(prev_frame() == end_frame())
      return std::nullopt;

    if(rate <= 0.)
      return prev_frame();

    const double musical_tick_duration
        = end_position_in_quarters - start_position_in_quarters;
    if(musical_tick_duration <= 0.)
      return prev_frame();

    if(rate <= 1.)
    {
      return get_quantification_date_for_bars_or_longer(rate);
    }
    else
    {
      return get_quantification_date_for_shorter_than_bars(rate);
    }
  }

  // Given a quantification rate (1 for bars, 2 for half, 4 for quarters...)
  // return the next occurring quantification date, if such date is in the tick
  // defined by this token_request: div == 1 means 1 bar.
  // Unlike the function above, this one also takes into account parent bar and time signature changes which
  // may or may not be desired depending on the situation.
  // FIXME does not seem to work either.
  [[nodiscard]] halp::quantification_frames
  get_quantification_date_with_bars(double rate) const noexcept
  {
    halp::quantification_frames quantification_date;

    if(rate <= 0.)
      return {}; //fixme: tk.prev_date;

    const double musical_tick_duration
        = this->end_position_in_quarters - this->start_position_in_quarters;
    if(musical_tick_duration <= 0.)
      return {}; //fixme: this->prev_date;

    if(rate <= 1.)
    {
      // Quantize relative to bars
      if(this->bar_at_end != this->bar_at_start)
      {
        // 4 if we're in 4/4 for instance
        const double musical_bar_duration = this->bar_at_end - this->bar_at_start;

        // rate = 0.5 -> 2 bars at 3/4 -> 6 quarter notes
        const double quantif_duration = musical_bar_duration / rate;

        // we must be on quarter note 6, 12, 18, ... from the previous
        // signature
        const double rem = std::fmod(
            this->bar_at_end - this->last_signature_change, quantif_duration);
        if(rem < 0.0001)
        {
          // There is a bar change in this tick and it is when we are going to
          // trigger
          const double musical_bar_start
              = this->bar_at_end - this->start_position_in_quarters;

          const double ratio = musical_bar_start / musical_tick_duration;
          const int dt = this->frames; // TODO should be tick_offset

          // FIXME we should go "back" by as many measures length if length(bar) < length(tick)
          // as right now we only catch the last bar change, but there may be multiple bar changes,
          // with very fast tempos and long buffer sizes
          quantification_date.push_back({std::floor(dt * ratio), 1});
        }
      }
      else if(start_position_in_quarters == 0.0)
      {
        // Special first bar case
        return {{0, 1}};
      }
    }
    else
    {
      // Quantize relative to quarter divisions
      // TODO ! if there is a bar change,
      // and no prior quantization date before that, we have to quantize to the
      // bar change. To be handled by the host by splitting the buffer there.
      const double start_quarter
          = (this->start_position_in_quarters - this->bar_at_start);
      const double end_quarter = (this->end_position_in_quarters - this->bar_at_start);

      // duration of what we quantify in terms of quarters
      const double musical_quant_dur = rate / 4.;
      const double start_quant = std::floor(start_quarter * musical_quant_dur);
      const double end_quant = std::floor(end_quarter * musical_quant_dur);

      if(start_quant != end_quant)
      {
        if(end_quant == end_quarter * musical_quant_dur)
        {
          // We want quantization on start, not on end
          return {};
        }
        // Date to quantify is the next one :
        const double musical_tick_duration
            = this->end_position_in_quarters - this->start_position_in_quarters;
        const double ratio = this->frames / musical_tick_duration;

        int i = 1;
        for(;;)
        {
          const double quantified_duration
              = (this->bar_at_start + (start_quant + i) * 4. / rate)
                - this->start_position_in_quarters;

          if(int frame = std::floor(quantified_duration * ratio); frame < this->frames)
          {
            quantification_date.push_back({frame, 1});
            i++;
          }
          else
          {
            break;
          }
        }
      }
      else if(start_quant == start_quarter * musical_quant_dur)
      {
        // Special first bar case
        return {{0, 1}};
      }
    }

    return quantification_date;
  }

  template <typename Tick, typename Tock>
  constexpr void metronome(Tick tick, Tock tock) const noexcept
  {
    if((bar_at_end != bar_at_start) || start_position_in_quarters == 0.)
    {
      // There is a bar change in this tick, start the up tick
      const double musical_tick_duration
          = end_position_in_quarters - start_position_in_quarters;
      if(musical_tick_duration != 0)
      {
        const double musical_bar_start = bar_at_end - start_position_in_quarters;
        if(this->frames > 0)
        {
          const double ratio = musical_bar_start / musical_tick_duration;
          const int64_t hi_start_sample = this->frames * ratio;
          tick(hi_start_sample);
        }
      }
    }
    else
    {
      const int64_t start_quarter
          = std::floor(start_position_in_quarters - bar_at_start);
      const int64_t end_quarter = std::floor(end_position_in_quarters - bar_at_start);
      if(start_quarter != end_quarter)
      {
        // There is a quarter change in this tick, start the down tick
        // start_position is prev_date
        // end_position is date
        const double musical_tick_duration
            = end_position_in_quarters - start_position_in_quarters;
        if(musical_tick_duration != 0)
        {
          const double musical_bar_start
              = (end_quarter + bar_at_start) - start_position_in_quarters;
          if(this->frames > 0)
          {
            const double ratio = musical_bar_start / musical_tick_duration;
            const int64_t lo_start_sample = this->frames * ratio;
            tock(lo_start_sample);
          }
        }
      }
    }
  }

  // FIXME dpes that work for a bar change at frame 0 or 1 ?
  [[nodiscard]] constexpr bool unexpected_bar_change() const noexcept
  {
    double bar_difference = bar_at_end - bar_at_start;
    if(bar_difference != 0.)
    {
      // If the difference is divisble by the signature,
      // then the bar change is expected.
      // e.g. start = 4 -> end = 8  ; signature = 4/4 : good
      // e.g. start = 4 -> end = 8  ; signature = 6/8 : bad
      // e.g. start = 4 -> end = 7  ; signature = 6/8 : good

      double quarters_sig = 4. * double(signature.num) / signature.denom;
      double div = bar_difference / quarters_sig;
      bool unexpected = div - int64_t(div) > 0.000001;
      return unexpected;
    }
    return false;
  }

  //! Is the given value in this buffer
  [[nodiscard]] constexpr bool in_range(int64_t abs_time) const noexcept
  {
    return abs_time >= position_in_frames && abs_time < (position_in_frames + frames);
  }
};

struct tick_flicks : halp::tick_musical
{
  int64_t start_in_flicks{};
  int64_t end_in_flicks{};
  double relative_position{};
  int64_t parent_duration{};

  //! How much we read from our data model
  [[nodiscard]] constexpr int64_t model_read_duration() const noexcept
  {
    return end_in_flicks - start_in_flicks;
  }
};

struct setup
{
  int input_channels{};
  int output_channels{};
  int frames{};
  double rate{};
};
}

namespace halp
{
template <static_string lit, typename FP, static_string Desc = "">
struct audio_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }

  FP** samples{};
  int channels{};

  HALP_INLINE_FLATTEN operator avnd::span<FP*>() const noexcept
  {
    return {samples, channels};
  }
  HALP_INLINE_FLATTEN avnd::span<FP>
  channel(std::size_t i, std::size_t frames) const noexcept
  {
    return {samples[i], frames};
  }
};

template <static_string lit>
using audio_input_bus = audio_bus<lit, const double>;
template <static_string lit>
using audio_output_bus = audio_bus<lit, double>;
}

namespace halp
{

template <typename T>
struct sample_type_from_channels_t;

template <typename T>
struct sample_type_from_channels_t<T**>
{
  using value_type = T;
};

template <typename T>
struct pointer_to_member_reflection_t;

template <typename T, typename C>
struct pointer_to_member_reflection_t<T C::*>
{
  using member_type = T;
};

template <auto memfun>
using pointer_to_member_type =
    typename pointer_to_member_reflection_t<decltype(memfun)>::member_type;

// Given &inputs::audio, gives us the type of the "samples" member variable of audio
template <auto memfun>
using sample_type_from_channels
    = std::remove_const_t<typename sample_type_from_channels_t<
        std::decay_t<decltype(pointer_to_member_type<memfun>{}.samples)>>::value_type>;

template <static_string lit, auto mimicked_channel>
struct mimic_audio_bus
    : public dynamic_audio_bus<lit, sample_type_from_channels<mimicked_channel>>
{
  static constexpr auto mimick_channel = mimicked_channel;
};
}
