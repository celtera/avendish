#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/span_polyfill.hpp>
#include <boost/container/small_vector.hpp>
#include <cmath>
#include <halp/inline.hpp>
#include <halp/modules.hpp>
#include <halp/static_string.hpp>

#include <algorithm>
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
struct fixed_audio_frame
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }
  static constexpr int channels() { return WantedChannels; }

  FP frame[WantedChannels]{};
  HALP_INLINE_FLATTEN operator FP*() const noexcept { return frame; }
  HALP_INLINE_FLATTEN auto get() const noexcept { return *this; }
  HALP_INLINE_FLATTEN const FP& operator[](int i) const noexcept { return frame[i]; }
  HALP_INLINE_FLATTEN FP& operator[](int i) noexcept { return frame[i]; }
  HALP_INLINE_FLATTEN FP channel(std::size_t i) const noexcept
  {
    if(i < WantedChannels)
      return frame[i];
    else
      return {};
  }

  FP* begin() noexcept { return frame; }
  FP* end() noexcept { return frame + WantedChannels; }
  const FP* begin() const noexcept { return frame; }
  const FP* end() const noexcept { return frame + WantedChannels; }
  const FP* cbegin() noexcept { return frame; }
  const FP* cend() noexcept { return frame + WantedChannels; }
  const FP* cbegin() const noexcept { return frame; }
  const FP* cend() const noexcept { return frame + WantedChannels; }
};

template <typename FP>
struct dynamic_audio_frame_base
{
  FP* frame{};
  int channels{};

  HALP_INLINE_FLATTEN operator FP**() const noexcept { return frame; }
  HALP_INLINE_FLATTEN auto get() const noexcept { return *this; }
  HALP_INLINE_FLATTEN const FP& operator[](int i) const noexcept { return frame[i]; }
  HALP_INLINE_FLATTEN FP& operator[](int i) noexcept { return frame[i]; }
  HALP_INLINE_FLATTEN FP channel(std::size_t i, std::size_t frames) const noexcept
  {
    if(i < channels)
      return frame[i];
    else
      return {};
  }

  FP* begin() noexcept { return frame; }
  FP* end() noexcept { return frame + channels; }
  FP* begin() const noexcept { return frame; }
  FP* end() const noexcept { return frame + channels; }
  FP* cbegin() noexcept { return frame; }
  FP* cend() noexcept { return frame + channels; }
  FP* cbegin() const noexcept { return frame; }
  FP* cend() const noexcept { return frame + channels; }
};

template <static_string Name, typename FP, static_string Desc = "">
struct dynamic_audio_frame : dynamic_audio_frame_base<FP>
{
  static consteval auto name() { return std::string_view{Name.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }
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

  FP** begin() noexcept { return samples; }
  FP** end() noexcept { return samples + channels; }
  FP* const* begin() const noexcept { return samples; }
  FP* const* end() const noexcept { return samples + channels; }
  FP* const* cbegin() noexcept { return samples; }
  FP* const* cend() noexcept { return samples + channels; }
  FP* const* cbegin() const noexcept { return samples; }
  FP* const* cend() const noexcept { return samples + channels; }
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
  constexpr int pos_to_frame(double in_bar) const noexcept
  {
    const double start = start_position_in_quarters;
    const double musical_pos = in_bar + bar_at_start;
    const double end = end_position_in_quarters;

    const double duration = end - start;
    if(duration == 0. || frames <= 0)
      return 0;

    // Rewinding, end is before start and the numerator is negative too, so the
    // ratio still grows with the buffer position.
    const double percent = (musical_pos - start) / duration;
    const int f = percent * this->frames;
    return std::clamp(f, 0, frames - 1);
  }
  constexpr int64_t prev_frame() const noexcept { return position_in_frames; }
  constexpr int64_t end_frame() const noexcept { return position_in_frames + frames; }
  //! Calls fn(bar_line, next_bar_line) for every bar segment the tick touches,
  //! in increasing musical order.
  //!
  //! Bar lines come from two places and both matter: the ones the signature
  //! implies, and the one reported for the far end of the tick. A signature
  //! change puts a bar line where the arithmetic alone would not, and the grid
  //! restarts there.
  //!
  //! Kept identical to ossia::token_request::for_each_bar_segment: a plugin and
  //! a native node on the same score have to snap to the same grid.
  template <typename F>
  void for_each_bar_segment(double lo, double hi, F&& fn) const noexcept
  {
    const bool valid_sig = signature.num > 0 && signature.denom > 0;
    const double quarters_in_bar
        = valid_sig ? 4. * signature.num / signature.denom : 4.;
    if(!(quarters_in_bar > 0.))
      return;

    constexpr double eps = 1e-9;
    const bool rewinding = end_position_in_quarters < start_position_in_quarters;
    const double near_bar = rewinding ? bar_at_end : bar_at_start;
    const double far_bar = rewinding ? bar_at_start : bar_at_end;

    double b = near_bar;
    for(int i = 0; i < 1024 && b < far_bar - eps; i++)
    {
      const double next
          = (b + quarters_in_bar < far_bar) ? b + quarters_in_bar : far_bar;
      if(next > lo + eps && b < hi + eps)
        fn(b, next);
      b += quarters_in_bar;
    }

    b = (far_bar > near_bar) ? far_bar : near_bar;
    for(int i = 0; i < 1024 && b < hi + eps; i++)
    {
      fn(b, b + quarters_in_bar);
      b += quarters_in_bar;
    }
  }

  //! Every quantification point the tick crosses, in tick order, as a frame
  //! offset into the buffer and the index of the point within its bar.
  [[nodiscard]] quantification_frames get_quantification_date(double rate) const noexcept
  {
    quantification_frames res;

    if(prev_frame() == end_frame())
      return res;

    const double musical_tick_duration
        = end_position_in_quarters - start_position_in_quarters;
    const bool rewinding = musical_tick_duration < 0.;

    if(rate <= 0. || musical_tick_duration == 0.)
    {
      res.emplace_back(0, 0);
      return res;
    }

    const bool valid_sig = signature.num > 0 && signature.denom > 0;
    const double quarters_in_bar
        = valid_sig ? 4. * signature.num / signature.denom : 4.;
    constexpr double eps = 1e-9;

    // A point falling exactly on the end of the tick belongs to the next one,
    // so the interval is half-open at the end the tick heads towards.
    const auto try_push = [&](double musical_position, int index) {
      const double ratio
          = (musical_position - start_position_in_quarters) / musical_tick_duration;
      int f = int(std::floor(ratio * this->frames));
      if(f < 0)
        f = 0;
      if(f >= this->frames)
        return false;
      res.emplace_back(f, index);
      return res.size() < 1024;
    };

    const double lo = rewinding ? end_position_in_quarters : start_position_in_quarters;
    const double hi = rewinding ? start_position_in_quarters : end_position_in_quarters;

    if(rate <= 1.)
    {
      // A bar or longer: counted from the last signature change, and no bar
      // line subdivides it.
      const double unit = quarters_in_bar / rate;
      if(!(unit > 0.))
        return res;

      const double origin = last_signature_change;
      const double start = (start_position_in_quarters - origin) / unit;
      const double end = (end_position_in_quarters - origin) / unit;
      const int64_t first = rewinding ? int64_t(std::floor(start + eps))
                                      : int64_t(std::ceil(start - eps));

      for(int64_t k = first; rewinding ? (k > end + eps) : (k < end - eps);
          k += rewinding ? -1 : 1)
      {
        if(!try_push(k * unit + origin, int(k)))
          break;
      }
      return res;
    }

    // Shorter than a bar: a subdivision of the quarter note, counted from the
    // bar it falls in, so the grid restarts at every bar line.
    const double unit = 4. / rate;
    if(!(unit > 0.))
      return res;

    boost::container::small_vector<std::pair<double, double>, 8> segments;
    for_each_bar_segment(lo, hi, [&](double bar_line, double next_bar) {
      if(segments.size() < 1024)
        segments.emplace_back(bar_line, next_bar);
    });

    const auto walk_segment = [&](double bar_line, double next_bar) {
      const int divs = int(std::ceil((next_bar - bar_line) / unit)) + 1;
      if(!rewinding)
      {
        for(int k = 0; k <= divs; k++)
        {
          const double p = bar_line + k * unit;
          if(p >= next_bar - eps || p > hi + eps)
            return true;
          if(p < lo)
            continue;
          if(!try_push(p, k))
            return false;
        }
      }
      else
      {
        for(int k = divs; k >= 0; k--)
        {
          const double p = bar_line + k * unit;
          if(p >= next_bar - eps || p > hi + eps)
            continue;
          if(p < lo)
            return true;
          if(!try_push(p, k))
            return false;
        }
      }
      return true;
    };

    if(!rewinding)
    {
      for(const auto& s : segments)
        if(!walk_segment(s.first, s.second))
          break;
    }
    else
    {
      for(auto it = segments.rbegin(); it != segments.rend(); ++it)
        if(!walk_segment(it->first, it->second))
          break;
    }
    return res;
  }

  //! The first quantification point of the tick, if any.
  //!
  //! This is the first of get_quantification_date(), not a second
  //! implementation of it: a node that takes one point and a node that takes
  //! them all have to agree about where the grid is.
  [[nodiscard]] std::optional<int64_t>
  get_one_quantification_date(double rate) const noexcept
  {
    if(prev_frame() == end_frame())
      return std::nullopt;

    // Quantized triggers are not interactive while rewinding.
    if(end_position_in_quarters < start_position_in_quarters)
      return std::nullopt;

    const auto pts = get_quantification_date(rate);
    if(pts.empty())
      return std::nullopt;
    return int64_t(pts[0].first);
  }

  //! Like get_quantification_date, but each point says whether it is a bar line
  //! rather than which subdivision it is.
  [[nodiscard]] halp::quantification_frames
  get_quantification_date_with_bars(double rate) const noexcept
  {
    halp::quantification_frames res = get_quantification_date(rate);
    if(rate <= 0.)
      return res;

    // For a bar or longer every point is a bar line by construction; shorter
    // than that, index 0 is the one sitting on the bar line.
    for(auto& pt : res)
      pt.second = (rate <= 1. || pt.second == 0) ? 1 : 0;
    return res;
  }

  //! Reports every grid point the tick crosses, in order: bar lines through
  //! tick(), the quarters between them through tock(). The same grid the
  //! quantification dates use, so a click and a quantized event at one bar line
  //! land on the same frame.
  template <typename Tick, typename Tock>
  void metronome(Tick tick, Tock tock) const noexcept
  {
    const double musical_tick_duration
        = end_position_in_quarters - start_position_in_quarters;
    if(musical_tick_duration == 0. || this->frames <= 0)
      return;

    const bool rewinding = musical_tick_duration < 0.;
    const double lo = rewinding ? end_position_in_quarters : start_position_in_quarters;
    const double hi = rewinding ? start_position_in_quarters : end_position_in_quarters;

    const auto frame_of = [&](double musical_position) {
      const double ratio
          = (musical_position - start_position_in_quarters) / musical_tick_duration;
      int64_t f = int64_t(std::floor(ratio * this->frames));
      if(f < 0)
        f = 0;
      if(f >= this->frames)
        f = this->frames - 1;
      return f;
    };

    // A point sitting exactly on a tick boundary belongs to the tick that
    // starts on it, where it is frame 0, not to the one that ends on it.
    const auto emit = [&](double p, bool is_bar) {
      if(rewinding ? (p > hi || p <= lo) : (p < lo || p >= hi))
        return;
      if(is_bar)
        tick(frame_of(p));
      else
        tock(frame_of(p));
    };

    double seg_lo[64]{};
    double seg_hi[64]{};
    int n_seg = 0;
    for_each_bar_segment(lo, hi, [&](double bar_line, double next_bar) {
      if(n_seg < 64)
      {
        seg_lo[n_seg] = bar_line;
        seg_hi[n_seg] = next_bar;
        n_seg++;
      }
    });

    const auto walk_segment = [&](double bar_line, double next_bar) {
      if(!rewinding)
      {
        emit(bar_line, true);
        for(double q = bar_line + 1.; q < next_bar - 1e-9; q += 1.)
          emit(q, false);
      }
      else
      {
        double last = bar_line;
        for(double q = bar_line + 1.; q < next_bar - 1e-9; q += 1.)
          last = q;
        for(double q = last; q > bar_line + 1e-9; q -= 1.)
          emit(q, false);
        emit(bar_line, true);
      }
    };

    if(!rewinding)
      for(int i = 0; i < n_seg; i++)
        walk_segment(seg_lo[i], seg_hi[i]);
    else
      for(int i = n_seg - 1; i >= 0; i--)
        walk_segment(seg_lo[i], seg_hi[i]);
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
