#pragma once

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/smoothers.hpp>

#include <algorithm>
#include <array>
#include <deque>
#include <optional>

namespace ao
{

struct TimecodeSynchronizer
{
  halp_meta(name, "Timecode Synchronizer")
  halp_meta(c_name, "smooth_timecode_synchronizer")
  halp_meta(category, "Control/Timing")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(
      description,
      "Converts timecode and/or speed inputs into smooth speed variations "
      "with minimal position jumps, optimized for audio resampling.")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/timecode-synchronizer.html")
  halp_meta(uuid, "d479d5cc-d4cb-4180-80b1-44ec1786bca3")

  enum TimeFormat
  {
    Seconds,
    Milliseconds,
    Microseconds,
    Nanoseconds,
    Flicks
  };

  enum SyncMode
  {
    Both,
    Speed,
    Timecode,
  };

  enum NoSyncBehaviour
  {
    Stall,
    Zero,
    KeepSpeed,
  };

  struct
  {
    // Input values
    halp::val_port<"Timecode", double> estimatedTimecode{0.0};
    halp::val_port<"Speed", double> estimatedSpeed{1.0};
    halp::val_port<"Validity", bool> validInput{false};

    // Sync configuration
    halp::combobox_t<"Sync Mode", SyncMode> syncMode{SyncMode::Both};
    halp::combobox_t<"Input format", TimeFormat> inputFormat{TimeFormat::Seconds};
    halp::combobox_t<"If no sync", NoSyncBehaviour> noSyncBehaviour{Stall};

    // Algorithm parameters
    halp::knob_f32<"Jump threshold", halp::range{0.001, 10, 1.}> positionJumpThreshold{
        1.0};
    halp::knob_f32<"Max speed correction", halp::range{0., 10., 1.}> maxSpeedCorrection{
        0.1};
    halp::knob_f32<"Correction time constant", halp::range{0.001, 100., 1.}>
        correctionTimeConstant{1.0};
    halp::knob_f32<"Smoothing factor", halp::range{0., 1., 0.95}> speedSmoothingFactor{
        0.95};
    halp::knob_f32<"Dead reckoning timeout", halp::range{0., 10., 1.}>
        deadReckoningTimeout{2.0};

    // Timecode-only mode parameters
    halp::knob_f32<"Speed estimation window", halp::range{0.01, 2., 0.1}>
        speedEstimationWindow{0.1}; // seconds
    halp::knob_f32<"Speed filter strength", halp::range{0., 1., 0.8}>
        speedFilterStrength{0.8}; // for timecode-only mode

    halp::combobox_t<"Output format", TimeFormat> outputFormat{TimeFormat::Seconds};
  } inputs;

  struct
  {
    halp::val_port<"Speed", double> speed{1.0};
    halp::val_port<"Tempo", double> tempo{120.0};
    halp::val_port<"Timecode", std::optional<double>> timecode{};
    halp::val_port<"Estimated Speed", double> estimatedSpeed{1.0};
  } outputs;

  halp::setup config;
  void prepare(halp::setup s)
  {
    config = s;
    // Clear history when setup changes
    m_timecode_history.clear();
  }

  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks tk)
  {
    if(config.rate <= 0)
      return;

    auto deltaTime = tk.frames / config.rate;
    m_system_time += deltaTime;

    if(!m_initialized)
    {
      initialize();
      outputs.speed = m_current_speed;
      outputs.tempo = currentOutputTempo();
      outputs.timecode = currentOutputTimecode(m_current_position);
      outputs.estimatedSpeed = m_derived_speed;
      return;
    }

    // Update our position based on current speed
    m_current_position += m_current_speed * deltaTime;

    // Process based on sync mode
    bool hasValidInput = false;

    switch(inputs.syncMode)
    {
      case SyncMode::Speed:
        hasValidInput = processSpeedOnly(deltaTime);
        break;

      case SyncMode::Timecode:
        hasValidInput = processTimecodeOnly(deltaTime);
        break;

      case SyncMode::Both:
        hasValidInput = processTimecodeAndSpeed(deltaTime);
        break;
    }

    if(!hasValidInput)
    {
      processInvalidInput(deltaTime);
    }

    // Apply speed smoothing
    m_current_speed = inputs.speedSmoothingFactor * m_current_speed
                      + (1.0 - inputs.speedSmoothingFactor) * m_target_speed;

    // Update outputs
    outputs.speed = m_current_speed;
    outputs.tempo = currentOutputTempo();
    outputs.estimatedSpeed = m_derived_speed;
  }

  void reset()
  {
    m_current_position = 0.0;
    m_current_speed = 0.0;
    m_target_speed = 0.0;
    m_derived_speed = 1.0;
    m_last_valid_input_time = 0.0;
    m_position_error = 0.0;
    m_initialized = false;
    m_in_dead_reckoning = false;
    m_system_time = 0.0;
    m_timecode_history.clear();
    m_last_timecode_value = 0.0;
    m_last_timecode_time = 0.0;
    m_has_last_timecode = false;
  }

private:
  // Timecode history entry for speed estimation
  struct TimecodeEntry
  {
    double timecode; // Timecode value
    double time;     // System time when received
  };

  double currentInputTimecode()
  {
    switch(inputs.inputFormat)
    {
      default:
      case Seconds:
        return inputs.estimatedTimecode;
      case Milliseconds:
        return inputs.estimatedTimecode / 1e3;
      case Microseconds:
        return inputs.estimatedTimecode / 1e6;
      case Nanoseconds:
        return inputs.estimatedTimecode / 1e9;
      case Flicks:
        return inputs.estimatedTimecode / 705'600'000.0;
    }
  }

  double currentOutputTimecode(double v)
  {
    switch(inputs.outputFormat)
    {
      default:
      case Seconds:
        return v;
      case Milliseconds:
        return v * 1e3;
      case Microseconds:
        return v * 1e6;
      case Nanoseconds:
        return v * 1e9;
      case Flicks:
        return v * 705'600'000.0;
    }
  }

  double currentOutputTempo() { return m_current_speed * 120.; }

  void initialize()
  {
    if(inputs.validInput)
    {
      m_current_position = currentInputTimecode();

      switch(inputs.syncMode)
      {
        case SyncMode::Speed:
          m_current_speed = inputs.estimatedSpeed;
          m_target_speed = inputs.estimatedSpeed;
          break;

        case SyncMode::Timecode:
          // Start with normal speed, will derive from timecode changes
          m_current_speed = 1.0;
          m_target_speed = 1.0;
          m_last_timecode_value = m_current_position;
          m_last_timecode_time = m_system_time;
          m_has_last_timecode = true;
          break;

        case SyncMode::Both:
          m_current_speed = inputs.estimatedSpeed;
          m_target_speed = inputs.estimatedSpeed;
          break;
      }

      m_initialized = true;
    }
  }

  bool processSpeedOnly(double deltaTime)
  {
    if(!inputs.validInput)
      return false;

    // Simply follow the input speed
    m_target_speed = inputs.estimatedSpeed;
    m_last_valid_input_time = 0.0;
    m_in_dead_reckoning = false;

    return true;
  }

  bool processTimecodeOnly(double deltaTime)
  {
    if(!inputs.validInput)
      return false;

    double currentTimecode = currentInputTimecode();

    // Add to history
    m_timecode_history.push_back({currentTimecode, m_system_time});

    // Remove old entries outside the estimation window
    while(!m_timecode_history.empty()
          && (m_system_time - m_timecode_history.front().time)
                 > inputs.speedEstimationWindow)
    {
      m_timecode_history.pop_front();
    }

    // Estimate speed from timecode history
    if(m_timecode_history.size() >= 2)
    {
      // Use linear regression or simple slope calculation
      double estimatedSpeed = estimateSpeedFromHistory();

      // Apply filtering to smooth the estimated speed
      m_derived_speed = inputs.speedFilterStrength * m_derived_speed
                        + (1.0 - inputs.speedFilterStrength) * estimatedSpeed;

      // Now use this derived speed for position correction
      double positionError = currentTimecode - m_current_position;

      if(std::abs(positionError) > inputs.positionJumpThreshold)
      {
        // Large error: jump to new position
        m_current_position = currentTimecode;
        outputs.timecode = currentOutputTimecode(m_current_position);
        m_target_speed = m_derived_speed;
        m_position_error = 0.0;
      }
      else
      {
        // Small error: correct via speed adjustment
        double baseSpeed = m_derived_speed;
        double correctionSpeed = positionError / inputs.correctionTimeConstant;

        // Limit correction magnitude
        double maxCorrection = inputs.maxSpeedCorrection * std::abs(baseSpeed);
        correctionSpeed = std::clamp(correctionSpeed, -maxCorrection, maxCorrection);

        m_target_speed = baseSpeed + correctionSpeed;
        m_position_error = positionError;
      }
    }
    else if(m_has_last_timecode)
    {
      // Simple two-point speed estimation for initial samples
      double timeDiff = m_system_time - m_last_timecode_time;
      if(timeDiff > 0.001) // Avoid division by very small numbers
      {
        double timecodeDiff = currentTimecode - m_last_timecode_value;
        m_derived_speed = timecodeDiff / timeDiff;
        m_target_speed = m_derived_speed;
      }
    }

    m_last_timecode_value = currentTimecode;
    m_last_timecode_time = m_system_time;
    m_has_last_timecode = true;
    m_last_valid_input_time = 0.0;
    m_in_dead_reckoning = false;

    return true;
  }

  bool processTimecodeAndSpeed(double deltaTime)
  {
    if(!inputs.validInput)
    {
      // Speed is generally still valid, just timecode must be ignored
      if(inputs.estimatedSpeed > -10. && inputs.estimatedSpeed < 10.)
      {
        m_target_speed = inputs.estimatedSpeed;
        m_last_valid_input_time = 0.0;
        return true;
      }
      else
      {
        return false;
      }
    }

    // When both inputs are available
    double newError = currentInputTimecode() - m_current_position;

    if(std::abs(newError) > inputs.positionJumpThreshold)
    {
      // Large error: jump to new position
      m_current_position = currentInputTimecode();
      outputs.timecode = currentOutputTimecode(m_current_position);
      m_target_speed = inputs.estimatedSpeed;
      m_position_error = 0.0;
    }
    else
    {
      // Small error: correct via speed adjustment
      double baseSpeed = inputs.estimatedSpeed;
      double correctionSpeed = newError / inputs.correctionTimeConstant;

      if(baseSpeed >= 0)
      {
        correctionSpeed = std::clamp(
            correctionSpeed, -inputs.maxSpeedCorrection * baseSpeed,
            inputs.maxSpeedCorrection * baseSpeed);
      }
      else
      {
        correctionSpeed = std::clamp(
            correctionSpeed, inputs.maxSpeedCorrection * baseSpeed,
            -inputs.maxSpeedCorrection * baseSpeed);
      }

      m_target_speed = baseSpeed + correctionSpeed;
      m_position_error = newError;
    }

    m_last_valid_input_time = 0.0;
    m_in_dead_reckoning = false;

    return true;
  }

  void processInvalidInput(double deltaTime)
  {
    m_last_valid_input_time += deltaTime;

    switch(inputs.noSyncBehaviour)
    {
      case NoSyncBehaviour::Zero:
        // Immediately stop when sync is lost
        m_target_speed = 0.0;
        m_in_dead_reckoning = true;
        break;

      case NoSyncBehaviour::Stall:
        // Dead reckoning until timeout, then stop
        if(m_last_valid_input_time > inputs.deadReckoningTimeout)
        {
          m_target_speed = 0.0;
          m_in_dead_reckoning = true;
        }
        // Otherwise maintain current speed (dead reckoning)
        break;

      case NoSyncBehaviour::KeepSpeed:
        // Maintain current speed indefinitely (true dead reckoning)
        // Never change target speed, just keep going
        m_in_dead_reckoning = true;
        break;
    }
  }

  double estimateSpeedFromHistory()
  {
    if(m_timecode_history.size() < 2)
      return 1.0;

    // Simple linear regression for speed estimation
    double sumT = 0.0, sumTC = 0.0, sumT2 = 0.0, sumTTC = 0.0;
    double t0 = m_timecode_history.front().time;

    for(const auto& entry : m_timecode_history)
    {
      double t = entry.time - t0; // Relative time
      double tc = entry.timecode;

      sumT += t;
      sumTC += tc;
      sumT2 += t * t;
      sumTTC += t * tc;
    }

    size_t n = m_timecode_history.size();
    double denominator = n * sumT2 - sumT * sumT;

    if(std::abs(denominator) < 1e-9) // Avoid division by zero
      return m_derived_speed;        // Keep previous estimate

    // Slope = speed
    double speed = (n * sumTTC - sumT * sumTC) / denominator;

    speed = std::clamp(speed, -10.0, 10.0);

    return speed;
  }

private:
  // Core state
  double m_current_position = 0.0;
  double m_current_speed = 1.0;
  double m_target_speed = 1.0;
  double m_last_valid_input_time = 0.0;
  double m_position_error = 0.0;

  // State flags
  bool m_initialized = false;
  bool m_in_dead_reckoning = false;

  // System time tracking
  double m_system_time = 0.0;

  // Timecode-only mode state
  std::deque<TimecodeEntry> m_timecode_history;
  double m_last_timecode_value = 0.0;
  double m_last_timecode_time = 0.0;
  bool m_has_last_timecode = false;
  double m_derived_speed = 1.0; // Speed derived from timecode changes
};

} // namespace ao
