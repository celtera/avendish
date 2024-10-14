#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/span_polyfill.hpp>
#include <boost/container/small_vector.hpp>
#include <cmath>
#include <halp/inline.hpp>
#include <halp/static_string.hpp>

#include <cstdint>
#include <functional>
#include <string_view>
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
  int frames{};

  double tempo = 120.;
  struct
  {
    int num;
    int denom;
  } signature;

  int64_t position_in_frames{};

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

  // If the division falls in the current tick, returns the corresponding frames
  // FIXME: this fails if we have a change of musical metric in the middle of a bar.
  // This requires the host to always cut the tick before a change of metrics
  // Div is the division in quarter notes: div == 1 means 1 quarter note.
  // FIXME: use std::generator :)
  [[nodiscard]] quantification_frames get_quantification_date(double div) const noexcept
  {
    quantification_frames frames;
    double start_in_bar = start_position_in_quarters - bar_at_start;
    double end_in_bar = end_position_in_quarters - bar_at_start;

    auto pos_to_frame = [this](double in_bar) {
      double start = start_position_in_quarters;
      double musical_pos = in_bar + bar_at_start;
      double end = end_position_in_quarters;

      double percent = (musical_pos - start) / (end - start);
      int f = percent * this->frames;
      return f;
    };
    double cur = start_in_bar;
    double q = std::floor(start_in_bar / div);
    if(std::abs(cur - q * div) < 1e-9)
    {
      // Add it: we're on 0 or the correct thing
      frames.emplace_back(pos_to_frame(cur), q);
    }
    else
    {
      cur = q * div;
    }

    for(;;)
    {
      cur += div;
      if(cur < end_in_bar)
      {
        frames.emplace_back(pos_to_frame(cur), q);
      }
      else
      {
        break;
      }
    }
    return frames;
  }

  // Given a quantification rate (1 for bars, 2 for half, 4 for quarters...)
  // return the next occurring quantification date, if such date is in the tick
  // defined by this token_request: div == 1 means 1 bar.
  // Unlike the function above, this one also takes into account parent bar and time signature changes which
  // may or may not be desired depending on the situatoin.
  // FIXME: use std::generator :)
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
      else if(
          this->start_position_in_quarters == 0. && this->end_position_in_quarters > 0.)
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
