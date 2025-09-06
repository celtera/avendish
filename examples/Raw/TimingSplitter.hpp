#pragma once
#include <halp/audio.hpp>

#include <optional>
namespace examples
{
struct TimingSplitter
{
  static consteval auto name() { return "Beat metronome"; }
  static consteval auto c_name() { return "avnd_timing_splitter"; }
  static consteval auto author() { return "Jean-Michaël Celerier"; }
  static consteval auto category() { return "Control/Timing"; }
  static consteval auto manual_url()
  {
    return "https://ossia.io/score-docs/processes/"
           "control-utilities.html#impulse-metronome";
  }
  static consteval auto description()
  {
    return "Outputs various musical timing subdivisions";
  }
  static consteval auto uuid() { return "4c9e6ca5-96c5-4df4-bca4-1e9693b98c9f"; }

  /*
  struct
  {
    struct
    {
      enum E
      {
        Parent,
        Custom
      } value{};
      enum widget
      {
        enumeration
      };
      struct range
      {
        std::string_view values[2]{"Parent", "Custom"};
        E init{};
      };
    } sync_mode;

    struct
    {
      enum widget
      {
        time_chooser
      };

      struct range
      {
        const float min = 0.0;
        const float max = 10.0;
        const float init = 1.0;
      };

      static constexpr auto name() { return "Time"; }

      float value = range{}.init;
    } duration;

  } inputs;
  */
  struct
  {
    struct
    {
      static constexpr auto name() { return "tick"; }
      std::optional<float> value;
    } tick;
    struct
    {
      static constexpr auto name() { return "tock"; }
      std::optional<float> value;
    } tock;
    struct
    {
      static constexpr auto name() { return "4 bars"; }
      std::optional<float> value;
    } bar_4;
    struct
    {
      static constexpr auto name() { return "2 bars"; }
      std::optional<float> value;
    } bar_2;
    struct
    {
      static constexpr auto name() { return "1 bar"; }
      std::optional<float> value;
    } bar_1;
    struct
    {
      static constexpr auto name() { return "half note"; }
      std::optional<float> value;
    } half;
    struct
    {
      static constexpr auto name() { return "4th note"; }
      std::optional<float> value;
    } quarter;
    struct
    {
      static constexpr auto name() { return "8th note"; }
      std::optional<float> value;
    } quaver;
    struct
    {
      static constexpr auto name() { return "16th note"; }
      std::optional<float> value;
    } semiquaver;
  } outputs;

  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks t)
  {
    t.metronome([&](int sample) { outputs.tick.value = 1.; }, [&](int sample) {
      outputs.tock.value = 1.;
    });
    for(auto r : t.get_quantification_date(16.))
      outputs.semiquaver.value = r.second;
    for(auto r : t.get_quantification_date(8.))
      outputs.quaver.value = r.second;
    for(auto r : t.get_quantification_date(4.))
      outputs.quarter.value = r.second;
    for(auto r : t.get_quantification_date(2.))
      outputs.half.value = r.second;
    for(auto r : t.get_quantification_date(1.))
      outputs.bar_1.value = r.second;
    for(auto r : t.get_quantification_date(0.5))
      outputs.bar_2.value = r.second;
    for(auto r : t.get_quantification_date(0.25))
      outputs.bar_4.value = r.second;
  }
};
}
