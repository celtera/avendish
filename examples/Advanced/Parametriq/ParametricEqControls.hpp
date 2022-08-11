#pragma once

// ********************************************************************************
//                              Widgets for the EQ
// ********************************************************************************
struct cutoffFrequency
{
  static constexpr auto name() { return "Cutoff Frequency"; }
  enum widget
  {
    knob
  };

  struct mapper
  {
    static float map(float x) { return std::pow(x, 1. / 10.); }
    static float unmap(float y) { return std::pow(y, 10.); }
  };
  struct range
  {
    static constexpr float min = 20.f;
    static constexpr float max = 20000.f;
    static constexpr float init = 100.f;
  };

  float value;
};

struct gain
{
  static constexpr auto name() { return "Gain"; }
  enum widget
  {
    knob
  };

  struct range
  {
    static constexpr float min = -120.f;
    static constexpr float max = 120.f;
    static constexpr float init = 0.1f;
  };

  float value;
};

struct centerFrequency
{
  static constexpr auto name() { return "Center Frequency"; }
  enum widget
  {
    knob
  };
  struct mapper
  {
    static float map(float x) { return std::pow(x, 1. / 10.); }
    static float unmap(float y) { return std::pow(y, 10.); }
  };
  struct range
  {
    static constexpr float min = 20.f;
    static constexpr float max = 20000.f;
    static constexpr float init = 100.f;
  };

  float value;
};

struct frequencyBandWidth
{
  static constexpr auto name() { return "Frequency Band Width"; }
  enum widget
  {
    knob
  };
  struct mapper
  {
    static float map(float x) { return std::pow(x, 1. / 10.); }
    static float unmap(float y) { return std::pow(y, 10.); }
  };
  struct range
  {
    static constexpr float min = 0.01f;
    static constexpr float max = 5000.f;
    static constexpr float init = 100.f;
  };

  float value;
};

struct passbandRipple
{
  static constexpr auto name() { return "Passband Ripple"; }
  enum widget
  {
    knob
  };
  struct range
  {
    static constexpr float min = 0.;
    static constexpr float max = 10.;
    static constexpr float init = 2.;
  };

  float value;
};

template <int m>
struct order
{
  static constexpr auto name() { return "Filter Order"; }
  struct range
  {
    static constexpr int min = 1;
    static constexpr int max = m;
    static constexpr int init = 1;
  };
  int value;
};

struct filterToggle
{
  enum widget
  {
    toggle
  };
  struct range
  {
    static constexpr bool off = false;
    static constexpr bool on = true;
    static constexpr bool init = false;
  };
  bool value;
  static constexpr auto name() { return "Apply filter"; }
}; // lowpassToggle, highpassToggle, bandpassToggle, bandstopToggle, lowshelfToggle, highshelfToggle, bandshelfToggle;

enum filters
{
  lowpass,
  highpass,
  bandpass,
  bandstop,
  lowshelf,
  highshelf,
  bandshelf,
  FILTERS_NB
};

template <typename T>
using my_pair = std::pair<std::string_view, T>;
struct EQBand
{
  enum widget
  {
    combobox
  };
  static constexpr auto name() { return "Filter Band"; }
  struct range
  {
    my_pair<filters> values[filters::FILTERS_NB]{
        {"Lowpass", filters::lowpass},    {"Highpass", filters::highpass},
        {"Bandpass", filters::bandpass},  {"Bandstop", filters::bandstop},
        {"Lowshelf", filters::lowshelf},  {"Highshelf", filters::highshelf},
        {"Bandshelf", filters::bandshelf}};
    filters init = filters::lowpass;
  };

  filters value{};
  int id{};
};