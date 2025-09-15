#pragma once

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <optional>

namespace ao
{
/**
 * @brief Advanced value filter combining range filtering, repetition filtering,
 *        threshold crossing detection, and hysteresis
 */
struct ValueFilter
{
public:
  halp_meta(name, "Value Filter")
  halp_meta(c_name, "value_filter")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(
      description,
      "Advanced filter for sensor values with range, repetition, hysteresis and "
      "crossing detection")
  halp_meta(uuid, "76db132f-ef86-45a8-b25d-160ad472718d")

  enum InputFilterMode
  {
    None,
    Outside,
    Inside,
    AboveMin,
    BelowMax
  };
  struct inputs_t
  {
    struct : halp::val_port<"Input", ossia::value>
    {

    } input;

    // Noise gate
    struct : halp::spinbox_f32<"Noise Gate Threshold", halp::range{0., 1e3, 0.0}>
    {
      halp_meta(description, "Minimum change required to pass through noise gate")
    } noise_threshold;

    // Range filtering controls
    struct : halp::enum_t<InputFilterMode, "Filter Range">
    {
      enum widget
      {
        combobox
      };
      halp_meta(description, "Enable filtering of values according to range")
    } filter_range;

    struct : halp::spinbox_f32<"Min", halp::range{-1e6, 1e6, 0.}>
    {
      halp_meta(description, "Minimum value for range filter")
    } min;

    struct : halp::spinbox_f32<"Max", halp::range{-1e6, 1e6, 1.}>
    {
      halp_meta(description, "Maximum value for range filter")
    } max;

    // Repetition filtering controls
    struct : halp::toggle<"Filter Repetitions">
    {
      halp_meta(description, "Enable filtering of repetitive values")
    } filter_repetitions;

    struct : halp::spinbox_f32<"Repetition filter range", halp::range{0., 1e3, 0.001}>
    {
      halp_meta(description, "Tolerance for considering values as repetitions")
    } repetition_tolerance;

    // Hysteresis controls
    struct : halp::toggle<"Enable Hysteresis">
    {
      halp_meta(description, "Enable hysteresis for threshold crossings")
    } enable_hysteresis;

    struct : halp::spinbox_f32<"Hysteresis", halp::range{0., 1., 0.05}>
    {
      halp_meta(description, "Hysteresis amount for threshold crossings")
    } hysteresis;

    // Hold time (in samples)
    struct : halp::spinbox_i32<"Hold Samples", halp::range{0, 10000, 0}>
    {
      halp_meta(description, "Hold time in samples after crossing thresholds")
    } hold_time;

    // Value clipping
    struct : halp::toggle<"Clip to Range">
    {
      halp_meta(description, "Clip output values to min/max range instead of filtering")
    } clip_to_range;

    // Value clipping
    struct : halp::toggle<"Temporal filter">
    {
      halp_meta(description, "Enable the temporal filter")
    } temporal_filter;
    struct : halp::time_chooser<"Temporal filter duration">
    {
      halp_meta(description, "Send a new value at most this regularly")
    } temporal_filter_t;

    // Reset button
    struct : halp::impulse_button<"Reset">
    {
      halp_meta(description, "Reset filter state")
    } reset;

  } inputs;

  struct outputs_t
  {
    // Filtered value output
    halp::val_port<"Filtered", std::optional<float>> filtered;

    // Crossing detection outputs
    halp::val_port<"Cross Min Up", std::optional<halp::impulse>> cross_min_up;
    halp::val_port<"Cross Min Down", std::optional<halp::impulse>> cross_min_down;
    halp::val_port<"Cross Max Up", std::optional<halp::impulse>> cross_max_up;
    halp::val_port<"Cross Max Down", std::optional<halp::impulse>> cross_max_down;

    // State indicators
    halp::val_port<"In Range", bool> in_range;
    halp::val_port<"Above High", bool> above_high;
    halp::val_port<"Below Low", bool> below_low;

  } outputs;

    std::optional<float> previous_value;
    std::optional<float> previous_output;
    std::optional<int64_t> previous_value_t;
    float smoothed_value = 0.f;

    // Crossing detection states
    bool was_below_min = false;
    bool was_above_max = false;

    // Hysteresis states
    float effective_min = 0.f;
    float effective_max = 1.f;
    bool min_crossed_up = false;
    bool max_crossed_up = false;

    // Hold time counters
    int min_hold_counter = 0;
    int max_hold_counter = 0;

    void reset(inputs_t& inputs)
    {
      if(inputs.max.value <= inputs.min.value)
        inputs.max.value = inputs.min.value + 0.0001;
      previous_value = std::nullopt;
      previous_output = std::nullopt;
      smoothed_value = 0.f;
      was_below_min = false;
      was_above_max = false;
      min_crossed_up = false;
      max_crossed_up = false;
      effective_min = inputs.min.value;
      effective_max = inputs.max.value;
      min_hold_counter = 0;
      max_hold_counter = 0;
    }

  void prepare()
  {
    if(inputs.max.value <= inputs.min.value)
      inputs.max.value = inputs.min.value + 0.0001;
    this->effective_min = inputs.min.value;
    this->effective_max = inputs.max.value;
  }

  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks tk) noexcept
  {
    if(inputs.reset)
      this->reset(inputs);

    if(inputs.max.value <= inputs.min.value)
      inputs.max.value = inputs.min.value + 0.0001;

    // Clear all impulse outputs
    outputs.cross_min_up.value = std::nullopt;
    outputs.cross_min_down.value = std::nullopt;
    outputs.cross_max_up.value = std::nullopt;
    outputs.cross_max_down.value = std::nullopt;

    if(!this->inputs.input.value.valid())
      return;

    float value = ossia::convert<float>(this->inputs.input.value);
    auto& input_value = value;

    this->smoothed_value = value;

    // Apply noise gate if enabled
    if(inputs.noise_threshold.value > 0. && this->previous_value)
    {
      float change = std::abs(value - *this->previous_value);
      if(change < inputs.noise_threshold.value)
      {
        value = *this->previous_value;
      }
    }

    // Update hysteresis thresholds
    // Proper hysteresis behavior:
    // - When value is BELOW threshold, use lower threshold (threshold - hyst) to detect crossing UP
    // - When value is ABOVE threshold, use higher threshold (threshold + hyst) to detect crossing DOWN
    if(inputs.enable_hysteresis)
    {
      float hyst = inputs.hysteresis.value;

      // Min threshold hysteresis
      // If we're currently below min (haven't crossed up), we need to reach min + hyst to cross up
      // If we're currently above min (have crossed up), we need to fall below min - hyst to cross down
      if(!this->min_crossed_up)
      {
        this->effective_min = inputs.min.value + hyst;
      }
      else
      {
        this->effective_min = inputs.min.value - hyst;
      }

      // Max threshold hysteresis
      // If we're currently below max (haven't crossed up), we need to reach max + hyst to cross up
      // If we're currently above max (have crossed up), we need to fall below max - hyst to cross down
      if(!this->max_crossed_up)
      {
        this->effective_max = inputs.max.value - hyst;
      }
      else
      {
        this->effective_max = inputs.max.value + hyst;
      }
    }
    else
    {
      this->effective_min = inputs.min.value;
      this->effective_max = inputs.max.value;
    }
    if(this->effective_max <= this->effective_min)
      this->effective_max = this->effective_min + 0.0001;

    // Detect min crossing with hold time
    bool below_min = value < this->effective_min;

    // Handle min threshold with hold time
    if(this->min_hold_counter > 0)
    {
      this->max_hold_counter = 0;
      this->min_hold_counter--;
      // During hold, maintain previous state
      below_min = this->was_below_min;
    }
    else
    {
      if(below_min && !this->was_below_min)
      {
        outputs.cross_min_down.value = halp::impulse{};
        this->min_crossed_up = false;
        this->min_hold_counter = inputs.hold_time.value;
        this->max_hold_counter = 0;
      }
      else if(!below_min && this->was_below_min)
      {
        outputs.cross_min_up.value = halp::impulse{};
        this->min_crossed_up = true;
        this->min_hold_counter = inputs.hold_time.value;
        this->max_hold_counter = 0;
      }
      this->was_below_min = below_min;
    }

    // Detect max crossing with hold time
    bool above_max = value > this->effective_max;

    // Handle max threshold with hold time
    if(this->max_hold_counter > 0)
    {
      this->min_hold_counter = 0;
      this->max_hold_counter--;
      // During hold, maintain previous state
      above_max = this->was_above_max;
    }
    else
    {
      if(above_max && !this->was_above_max)
      {
        outputs.cross_max_up.value = halp::impulse{};
        this->max_crossed_up = true;
        this->max_hold_counter = inputs.hold_time.value;
        this->min_hold_counter = 0;
      }
      else if(!above_max && this->was_above_max)
      {
        outputs.cross_max_down.value = halp::impulse{};
        this->max_crossed_up = false;
        this->max_hold_counter = inputs.hold_time.value;
        this->min_hold_counter = 0;
      }
      this->was_above_max = above_max;
    }

    // Update state indicators
    outputs.in_range.value = !this->was_below_min && !this->was_above_max;
    // Apply range filtering
    bool pass = true;
    if(this->min_hold_counter <= 0 & this->max_hold_counter <= 0)
    {
      if(inputs.clip_to_range)
      {
        switch(inputs.filter_range.value)
        {
          case InputFilterMode::None:
            break;
          case InputFilterMode::Inside:
            value = std::clamp(value, this->effective_min, this->effective_max);
            break;
          case InputFilterMode::Outside:
            if(outputs.in_range.value)
            {
              if(value <= (inputs.max.value + inputs.min.value) / 2)
                value = inputs.min.value;
              else
                value = inputs.max.value;
            }
            break;
          case InputFilterMode::AboveMin:
            value = std::max(value, this->effective_min);
            break;
          case InputFilterMode::BelowMax:
            value = std::min(value, this->effective_max);
            break;
        }
      }
      else
      {
        switch(inputs.filter_range.value)
        {
          case InputFilterMode::None:
            break;
          case InputFilterMode::Inside:
            pass = (value >= this->effective_min && value <= this->effective_max);
            break;
          case InputFilterMode::Outside:
            pass = (value <= this->effective_min || value >= this->effective_max);
            break;
          case InputFilterMode::AboveMin:
            pass = (value >= this->effective_min);
            break;
          case InputFilterMode::BelowMax:
            pass = (value <= this->effective_max);
            break;
        }
      }
    }

    // Check if value passes range filter
    if(!pass)
    {
      this->previous_value = input_value;
      outputs.filtered.value = std::nullopt;
      return;
    }

    // Apply repetition filtering
    if(inputs.filter_repetitions && this->previous_output)
    {
      float diff = std::abs(value - *this->previous_output);
      if(diff < inputs.repetition_tolerance.value)
      {
        this->previous_value = input_value;
        outputs.filtered.value = std::nullopt;
        return;
      }
    }

    // Apply temporal filtering
    if(inputs.temporal_filter && this->previous_value_t)
    {
      auto t = inputs.temporal_filter_t.value; // in seconds in model-space
      auto t_flicks = 705'600'000 * t;
      if(tk.start_in_flicks < *this->previous_value_t + t_flicks)
      {
        outputs.filtered.value = std::nullopt;
        return;
      }
    }

    // Value passes all filters
    this->previous_value = input_value;
    this->previous_value_t = tk.start_in_flicks;
    this->previous_output = value;
    outputs.filtered.value = value;
  }
};

}
