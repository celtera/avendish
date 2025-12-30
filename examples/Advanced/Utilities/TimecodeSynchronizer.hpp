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

struct SmoothTimecodeSynchronizer
{
  halp_meta(name, "Smooth Timecode Synchronizer")
  halp_meta(c_name, "smooth_timecode_synchronizer")
  halp_meta(category, "Control/Timing")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(
      description,
      "Converts timecode and/or speed inputs into smooth speed variations "
      "with minimal position jumps, optimized for audio resampling.")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/smooth-timecode-synchronizer.html")
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
    TimecodeAndSpeed,
    SpeedOnly,
    TimecodeOnly,
  };

  struct
  {
    // Input values
    halp::val_port<"Speed", double> estimatedSpeed{1.0};
    halp::val_port<"Timecode", double> estimatedTimecode{0.0};
    halp::val_port<"Validity", bool> validInput{false};

    // Sync configuration
    halp::combobox_t<"Sync Mode", SyncMode> syncMode{SyncMode::TimecodeAndSpeed};
    halp::combobox_t<"Input format", TimeFormat> inputFormat{TimeFormat::Seconds};

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
    halp::val_port<"Estimated Speed", double> estimatedSpeed{
        1.0}; // For debugging timecode-only mode
  } outputs;

  halp::setup config;
  void prepare(halp::setup s)
  {
    config = s;
    // Clear history when setup changes
    timecodeHistory_.clear();
  }

  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks tk)
  {
    if(config.rate <= 0)
      return;

    auto deltaTime = tk.frames / config.rate;
    systemTime_ += deltaTime;

    if(!initialized_)
    {
      initialize();
      outputs.speed = currentSpeed_;
      outputs.tempo = currentOutputTempo();
      outputs.timecode = currentOutputTimecode(currentPosition_);
      outputs.estimatedSpeed = derivedSpeed_;
      return;
    }

    // Update our position based on current speed
    currentPosition_ += currentSpeed_ * deltaTime;

    // Process based on sync mode
    bool hasValidInput = false;

    switch(inputs.syncMode)
    {
      case SyncMode::SpeedOnly:
        hasValidInput = processSpeedOnly(deltaTime);
        break;

      case SyncMode::TimecodeOnly:
        hasValidInput = processTimecodeOnly(deltaTime);
        break;

      case SyncMode::TimecodeAndSpeed:
        hasValidInput = processTimecodeAndSpeed(deltaTime);
        break;
    }

    if(!hasValidInput)
    {
      processInvalidInput(deltaTime);
    }

    // Apply speed smoothing
    currentSpeed_ = inputs.speedSmoothingFactor * currentSpeed_
                    + (1.0 - inputs.speedSmoothingFactor) * targetSpeed_;

    // Update outputs
    outputs.speed = currentSpeed_;
    outputs.tempo = currentOutputTempo();
    outputs.estimatedSpeed = derivedSpeed_;
  }

  void reset()
  {
    currentPosition_ = 0.0;
    currentSpeed_ = 1.0;
    targetSpeed_ = 1.0;
    derivedSpeed_ = 1.0;
    lastValidInputTime_ = 0.0;
    positionError_ = 0.0;
    initialized_ = false;
    inDeadReckoning_ = false;
    systemTime_ = 0.0;
    timecodeHistory_.clear();
    lastTimecodeValue_ = 0.0;
    lastTimecodeTime_ = 0.0;
    hasLastTimecode_ = false;
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

  double currentOutputTempo() { return currentSpeed_ * 120.; }

  void initialize()
  {
    if(inputs.validInput)
    {
      currentPosition_ = currentInputTimecode();

      switch(inputs.syncMode)
      {
        case SyncMode::SpeedOnly:
          currentSpeed_ = inputs.estimatedSpeed;
          targetSpeed_ = inputs.estimatedSpeed;
          break;

        case SyncMode::TimecodeOnly:
          // Start with normal speed, will derive from timecode changes
          currentSpeed_ = 1.0;
          targetSpeed_ = 1.0;
          lastTimecodeValue_ = currentPosition_;
          lastTimecodeTime_ = systemTime_;
          hasLastTimecode_ = true;
          break;

        case SyncMode::TimecodeAndSpeed:
          currentSpeed_ = inputs.estimatedSpeed;
          targetSpeed_ = inputs.estimatedSpeed;
          break;
      }

      initialized_ = true;
    }
  }

  bool processSpeedOnly(double deltaTime)
  {
    if(!inputs.validInput)
      return false;

    // Simply follow the input speed
    targetSpeed_ = inputs.estimatedSpeed;
    lastValidInputTime_ = 0.0;
    inDeadReckoning_ = false;

    return true;
  }

  bool processTimecodeOnly(double deltaTime)
  {
    if(!inputs.validInput)
      return false;

    double currentTimecode = currentInputTimecode();

    // Add to history
    timecodeHistory_.push_back({currentTimecode, systemTime_});

    // Remove old entries outside the estimation window
    while(!timecodeHistory_.empty()
          && (systemTime_ - timecodeHistory_.front().time)
                 > inputs.speedEstimationWindow)
    {
      timecodeHistory_.pop_front();
    }

    // Estimate speed from timecode history
    if(timecodeHistory_.size() >= 2)
    {
      // Use linear regression or simple slope calculation
      double estimatedSpeed = estimateSpeedFromHistory();

      // Apply filtering to smooth the estimated speed
      derivedSpeed_ = inputs.speedFilterStrength * derivedSpeed_
                      + (1.0 - inputs.speedFilterStrength) * estimatedSpeed;

      // Now use this derived speed for position correction
      double positionError = currentTimecode - currentPosition_;

      if(std::abs(positionError) > inputs.positionJumpThreshold)
      {
        // Large error: jump to new position
        currentPosition_ = currentTimecode;
        outputs.timecode = currentOutputTimecode(currentPosition_);
        targetSpeed_ = derivedSpeed_;
        positionError_ = 0.0;
      }
      else
      {
        // Small error: correct via speed adjustment
        double baseSpeed = derivedSpeed_;
        double correctionSpeed = positionError / inputs.correctionTimeConstant;

        // Limit correction magnitude
        if(baseSpeed >= 0)
        {
          correctionSpeed = std::clamp(
              correctionSpeed, -inputs.maxSpeedCorrection * std::abs(baseSpeed),
              inputs.maxSpeedCorrection * std::abs(baseSpeed));
        }
        else
        {
          correctionSpeed = std::clamp(
              correctionSpeed, -inputs.maxSpeedCorrection * std::abs(baseSpeed),
              inputs.maxSpeedCorrection * std::abs(baseSpeed));
        }

        targetSpeed_ = baseSpeed + correctionSpeed;
        positionError_ = positionError;
      }
    }
    else if(hasLastTimecode_)
    {
      // Simple two-point speed estimation for initial samples
      double timeDiff = systemTime_ - lastTimecodeTime_;
      if(timeDiff > 0.001) // Avoid division by very small numbers
      {
        double timecodeDiff = currentTimecode - lastTimecodeValue_;
        derivedSpeed_ = timecodeDiff / timeDiff;
        targetSpeed_ = derivedSpeed_;
      }
    }

    lastTimecodeValue_ = currentTimecode;
    lastTimecodeTime_ = systemTime_;
    hasLastTimecode_ = true;
    lastValidInputTime_ = 0.0;
    inDeadReckoning_ = false;

    return true;
  }

  bool processTimecodeAndSpeed(double deltaTime)
  {
    if(!inputs.validInput)
      return false;

    // When both inputs are available
    double newError = currentInputTimecode() - currentPosition_;

    if(std::abs(newError) > inputs.positionJumpThreshold)
    {
      // Large error: jump to new position
      currentPosition_ = currentInputTimecode();
      outputs.timecode = currentOutputTimecode(currentPosition_);
      targetSpeed_ = inputs.estimatedSpeed;
      positionError_ = 0.0;
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

      targetSpeed_ = baseSpeed + correctionSpeed;
      positionError_ = newError;
    }

    lastValidInputTime_ = 0.0;
    inDeadReckoning_ = false;

    return true;
  }

  void processInvalidInput(double deltaTime)
  {
    lastValidInputTime_ += deltaTime;

    if(lastValidInputTime_ > inputs.deadReckoningTimeout)
    {
      // Timeout: gradually return to normal speed
      targetSpeed_ = 1.0;
      inDeadReckoning_ = true;
    }
    // Otherwise maintain current speed (dead reckoning)
  }

  double estimateSpeedFromHistory()
  {
    if(timecodeHistory_.size() < 2)
      return 1.0;

    // Simple linear regression for speed estimation
    double sumT = 0.0, sumTC = 0.0, sumT2 = 0.0, sumTTC = 0.0;
    double t0 = timecodeHistory_.front().time;

    for(const auto& entry : timecodeHistory_)
    {
      double t = entry.time - t0; // Relative time
      double tc = entry.timecode;

      sumT += t;
      sumTC += tc;
      sumT2 += t * t;
      sumTTC += t * tc;
    }

    size_t n = timecodeHistory_.size();
    double denominator = n * sumT2 - sumT * sumT;

    if(std::abs(denominator) < 1e-9) // Avoid division by zero
      return derivedSpeed_;          // Keep previous estimate

    // Slope = speed
    double speed = (n * sumTTC - sumT * sumTC) / denominator;

    // Sanity check - limit to reasonable range
    speed = std::clamp(speed, -10.0, 10.0);

    return speed;
  }

private:
  // Core state
  double currentPosition_ = 0.0;
  double currentSpeed_ = 1.0;
  double targetSpeed_ = 1.0;
  double lastValidInputTime_ = 0.0;
  double positionError_ = 0.0;

  // State flags
  bool initialized_ = false;
  bool inDeadReckoning_ = false;

  // System time tracking
  double systemTime_ = 0.0;

  // Timecode-only mode state
  std::deque<TimecodeEntry> timecodeHistory_;
  double lastTimecodeValue_ = 0.0;
  double lastTimecodeTime_ = 0.0;
  bool hasLastTimecode_ = false;
  double derivedSpeed_ = 1.0; // Speed derived from timecode changes
};

} // namespace ao
