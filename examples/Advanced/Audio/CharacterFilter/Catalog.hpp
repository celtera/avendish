#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */
// SST filters++ 87a171bcba52c014850848a29174a54f0adc6606, sorted ModelConfig tuples.
// Serialized order: append new entries, never reorder existing models or variants.
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace ao::character_filter
{
struct Configuration
{
  std::string_view name;
  std::uint8_t passband, slope, drive, submodel;
  bool extra, bipolar;
};
struct Model
{
  std::string_view name;
  int sst_id;
  int begin, count;
};
inline constexpr std::array<Model, 15> models{{
  {"Vember Classic", 16, 0, 23},
  {"Vember Ladder", 24, 23, 4},
  {"OB-Xd 2-pole", 69, 27, 8},
  {"OB-Xd 4-pole", 64, 35, 6},
  {"OB-Xd Xpander", 71, 41, 15},
  {"K35", 48, 56, 12},
  {"Diode Ladder", 53, 68, 4},
  {"Vintage Ladder", 80, 72, 6},
  {"Cutoff Warp", 96, 78, 60},
  {"Resonance Warp", 101, 138, 40},
  {"Fast SVF", 32, 178, 9},
  {"Tri-pole", 112, 187, 12},
  {"Comb", 128, 199, 7},
  {"Sample & Hold", 133, 206, 1},
  {"Cascade SVF", 34, 207, 14},
}};
inline constexpr std::array<Configuration, 221> configurations{{
  // Vember Classic
  {"Lowpass 12 dB Standard", 4, 18, 16, 0, false, false},
  {"Lowpass 12 dB Clean", 4, 18, 18, 0, false, false},
  {"Lowpass 12 dB Driven", 4, 18, 20, 0, false, false},
  {"Lowpass 24 dB Standard", 4, 36, 16, 0, false, false},
  {"Lowpass 24 dB Clean", 4, 36, 18, 0, false, false},
  {"Lowpass 24 dB Driven", 4, 36, 20, 0, false, false},
  {"Highpass 12 dB Standard", 8, 18, 16, 0, false, false},
  {"Highpass 12 dB Clean", 8, 18, 18, 0, false, false},
  {"Highpass 12 dB Driven", 8, 18, 20, 0, false, false},
  {"Highpass 24 dB Standard", 8, 36, 16, 0, false, false},
  {"Highpass 24 dB Clean", 8, 36, 18, 0, false, false},
  {"Highpass 24 dB Driven", 8, 36, 20, 0, false, false},
  {"Bandpass 12 dB Standard", 12, 18, 16, 0, false, false},
  {"Bandpass 12 dB Clean", 12, 18, 18, 0, false, false},
  {"Bandpass 12 dB Driven", 12, 18, 20, 0, false, false},
  {"Bandpass 24 dB Standard", 12, 36, 16, 0, false, false},
  {"Bandpass 24 dB Clean", 12, 36, 18, 0, false, false},
  {"Bandpass 24 dB Driven", 12, 36, 20, 0, false, false},
  {"Notch 12 dB Standard", 16, 18, 16, 0, false, false},
  {"Notch 12 dB Mild", 16, 18, 24, 0, false, false},
  {"Notch 24 dB Standard", 16, 36, 16, 0, false, false},
  {"Notch 24 dB Mild", 16, 36, 24, 0, false, false},
  {"Allpass", 24, 0, 0, 0, false, false},
  // Vember Ladder
  {"Lowpass 6 dB", 4, 6, 0, 0, false, false},
  {"Lowpass 12 dB", 4, 18, 0, 0, false, false},
  {"Lowpass 18 dB", 4, 24, 0, 0, false, false},
  {"Lowpass 24 dB", 4, 36, 0, 0, false, false},
  // OB-Xd 2-pole
  {"Lowpass Standard", 4, 0, 16, 0, false, false},
  {"Lowpass Pushed", 4, 0, 80, 0, false, false},
  {"Highpass Standard", 8, 0, 16, 0, false, false},
  {"Highpass Pushed", 8, 0, 80, 0, false, false},
  {"Bandpass Standard", 12, 0, 16, 0, false, false},
  {"Bandpass Pushed", 12, 0, 80, 0, false, false},
  {"Notch Standard", 16, 0, 16, 0, false, false},
  {"Notch Pushed", 16, 0, 80, 0, false, false},
  // OB-Xd 4-pole
  {"Lowpass 6 dB", 4, 6, 0, 0, false, false},
  {"Lowpass 12 dB", 4, 18, 0, 0, false, false},
  {"Lowpass 18 dB", 4, 24, 0, 0, false, false},
  {"Lowpass 24 dB", 4, 36, 0, 0, false, false},
  {"Lowpass 24 dB (Legacy)", 4, 36, 0, 16, false, false},
  {"Lowpass Morph", 4, 64, 0, 0, true, false},
  // OB-Xd Xpander
  {"Lowpass 6 dB", 4, 6, 0, 0, false, false},
  {"Lowpass 12 dB", 4, 18, 0, 0, false, false},
  {"Lowpass 18 dB", 4, 24, 0, 0, false, false},
  {"Lowpass 24 dB", 4, 36, 0, 0, false, false},
  {"Highpass 6 dB", 8, 6, 0, 0, false, false},
  {"Highpass 12 dB", 8, 18, 0, 0, false, false},
  {"Highpass 18 dB", 8, 24, 0, 0, false, false},
  {"Bandpass 12 dB", 12, 18, 0, 0, false, false},
  {"Bandpass 12 dB/6 dB", 12, 19, 0, 0, false, false},
  {"Bandpass 18 dB/6 dB", 12, 25, 0, 0, false, false},
  {"Bandpass 24 dB", 12, 36, 0, 0, false, false},
  {"Notch 12 dB", 16, 18, 0, 0, false, false},
  {"Phaser 18 dB", 128, 24, 0, 0, false, false},
  {"Notch+LP 12 dB/6 dB", 146, 19, 0, 0, false, false},
  {"Phaser+LP 18 dB/6 dB", 148, 25, 0, 0, false, false},
  // K35
  {"Lowpass None", 4, 0, 48, 0, false, false},
  {"Lowpass Mild", 4, 0, 50, 0, false, false},
  {"Lowpass Moderate", 4, 0, 52, 0, false, false},
  {"Lowpass Heavy", 4, 0, 54, 0, false, false},
  {"Lowpass Extreme", 4, 0, 56, 0, false, false},
  {"Lowpass Continuous", 4, 0, 57, 0, true, false},
  {"Highpass None", 8, 0, 48, 0, false, false},
  {"Highpass Mild", 8, 0, 50, 0, false, false},
  {"Highpass Moderate", 8, 0, 52, 0, false, false},
  {"Highpass Heavy", 8, 0, 54, 0, false, false},
  {"Highpass Extreme", 8, 0, 56, 0, false, false},
  {"Highpass Continuous", 8, 0, 57, 0, true, false},
  // Diode Ladder
  {"Lowpass 6 dB", 4, 6, 0, 0, false, false},
  {"Lowpass 12 dB", 4, 18, 0, 0, false, false},
  {"Lowpass 18 dB", 4, 24, 0, 0, false, false},
  {"Lowpass 24 dB", 4, 36, 0, 0, false, false},
  // Vintage Ladder
  {"Lowpass Runge-Kutta", 4, 0, 0, 32, false, false},
  {"Lowpass Runge-Kutta Compensated", 4, 0, 0, 34, false, false},
  {"Lowpass Huovilainen", 4, 0, 0, 36, false, false},
  {"Lowpass Huovilainen Compensated", 4, 0, 0, 38, false, false},
  {"Lowpass Huovilainen 2010", 4, 0, 0, 39, false, false},
  {"Lowpass Huovilainen Compensated 2010", 4, 0, 0, 40, false, false},
  // Cutoff Warp
  {"Lowpass tanh 1 Stage", 4, 0, 64, 96, false, false},
  {"Lowpass tanh 2 Stage", 4, 0, 64, 97, false, false},
  {"Lowpass tanh 3 Stage", 4, 0, 64, 98, false, false},
  {"Lowpass tanh 4 Stage", 4, 0, 64, 99, false, false},
  {"Lowpass Soft Clip 1 Stage", 4, 0, 66, 96, false, false},
  {"Lowpass Soft Clip 2 Stage", 4, 0, 66, 97, false, false},
  {"Lowpass Soft Clip 3 Stage", 4, 0, 66, 98, false, false},
  {"Lowpass Soft Clip 4 Stage", 4, 0, 66, 99, false, false},
  {"Lowpass OJD 1 Stage", 4, 0, 68, 96, false, false},
  {"Lowpass OJD 2 Stage", 4, 0, 68, 97, false, false},
  {"Lowpass OJD 3 Stage", 4, 0, 68, 98, false, false},
  {"Lowpass OJD 4 Stage", 4, 0, 68, 99, false, false},
  {"Highpass tanh 1 Stage", 8, 0, 64, 96, false, false},
  {"Highpass tanh 2 Stage", 8, 0, 64, 97, false, false},
  {"Highpass tanh 3 Stage", 8, 0, 64, 98, false, false},
  {"Highpass tanh 4 Stage", 8, 0, 64, 99, false, false},
  {"Highpass Soft Clip 1 Stage", 8, 0, 66, 96, false, false},
  {"Highpass Soft Clip 2 Stage", 8, 0, 66, 97, false, false},
  {"Highpass Soft Clip 3 Stage", 8, 0, 66, 98, false, false},
  {"Highpass Soft Clip 4 Stage", 8, 0, 66, 99, false, false},
  {"Highpass OJD 1 Stage", 8, 0, 68, 96, false, false},
  {"Highpass OJD 2 Stage", 8, 0, 68, 97, false, false},
  {"Highpass OJD 3 Stage", 8, 0, 68, 98, false, false},
  {"Highpass OJD 4 Stage", 8, 0, 68, 99, false, false},
  {"Bandpass tanh 1 Stage", 12, 0, 64, 96, false, false},
  {"Bandpass tanh 2 Stage", 12, 0, 64, 97, false, false},
  {"Bandpass tanh 3 Stage", 12, 0, 64, 98, false, false},
  {"Bandpass tanh 4 Stage", 12, 0, 64, 99, false, false},
  {"Bandpass Soft Clip 1 Stage", 12, 0, 66, 96, false, false},
  {"Bandpass Soft Clip 2 Stage", 12, 0, 66, 97, false, false},
  {"Bandpass Soft Clip 3 Stage", 12, 0, 66, 98, false, false},
  {"Bandpass Soft Clip 4 Stage", 12, 0, 66, 99, false, false},
  {"Bandpass OJD 1 Stage", 12, 0, 68, 96, false, false},
  {"Bandpass OJD 2 Stage", 12, 0, 68, 97, false, false},
  {"Bandpass OJD 3 Stage", 12, 0, 68, 98, false, false},
  {"Bandpass OJD 4 Stage", 12, 0, 68, 99, false, false},
  {"Notch tanh 1 Stage", 16, 0, 64, 96, false, false},
  {"Notch tanh 2 Stage", 16, 0, 64, 97, false, false},
  {"Notch tanh 3 Stage", 16, 0, 64, 98, false, false},
  {"Notch tanh 4 Stage", 16, 0, 64, 99, false, false},
  {"Notch Soft Clip 1 Stage", 16, 0, 66, 96, false, false},
  {"Notch Soft Clip 2 Stage", 16, 0, 66, 97, false, false},
  {"Notch Soft Clip 3 Stage", 16, 0, 66, 98, false, false},
  {"Notch Soft Clip 4 Stage", 16, 0, 66, 99, false, false},
  {"Notch OJD 1 Stage", 16, 0, 68, 96, false, false},
  {"Notch OJD 2 Stage", 16, 0, 68, 97, false, false},
  {"Notch OJD 3 Stage", 16, 0, 68, 98, false, false},
  {"Notch OJD 4 Stage", 16, 0, 68, 99, false, false},
  {"Allpass tanh 1 Stage", 24, 0, 64, 96, false, false},
  {"Allpass tanh 2 Stage", 24, 0, 64, 97, false, false},
  {"Allpass tanh 3 Stage", 24, 0, 64, 98, false, false},
  {"Allpass tanh 4 Stage", 24, 0, 64, 99, false, false},
  {"Allpass Soft Clip 1 Stage", 24, 0, 66, 96, false, false},
  {"Allpass Soft Clip 2 Stage", 24, 0, 66, 97, false, false},
  {"Allpass Soft Clip 3 Stage", 24, 0, 66, 98, false, false},
  {"Allpass Soft Clip 4 Stage", 24, 0, 66, 99, false, false},
  {"Allpass OJD 1 Stage", 24, 0, 68, 96, false, false},
  {"Allpass OJD 2 Stage", 24, 0, 68, 97, false, false},
  {"Allpass OJD 3 Stage", 24, 0, 68, 98, false, false},
  {"Allpass OJD 4 Stage", 24, 0, 68, 99, false, false},
  // Resonance Warp
  {"Lowpass tanh 1 Stage", 4, 0, 64, 96, false, false},
  {"Lowpass tanh 2 Stage", 4, 0, 64, 97, false, false},
  {"Lowpass tanh 3 Stage", 4, 0, 64, 98, false, false},
  {"Lowpass tanh 4 Stage", 4, 0, 64, 99, false, false},
  {"Lowpass Soft Clip 1 Stage", 4, 0, 66, 96, false, false},
  {"Lowpass Soft Clip 2 Stage", 4, 0, 66, 97, false, false},
  {"Lowpass Soft Clip 3 Stage", 4, 0, 66, 98, false, false},
  {"Lowpass Soft Clip 4 Stage", 4, 0, 66, 99, false, false},
  {"Highpass tanh 1 Stage", 8, 0, 64, 96, false, false},
  {"Highpass tanh 2 Stage", 8, 0, 64, 97, false, false},
  {"Highpass tanh 3 Stage", 8, 0, 64, 98, false, false},
  {"Highpass tanh 4 Stage", 8, 0, 64, 99, false, false},
  {"Highpass Soft Clip 1 Stage", 8, 0, 66, 96, false, false},
  {"Highpass Soft Clip 2 Stage", 8, 0, 66, 97, false, false},
  {"Highpass Soft Clip 3 Stage", 8, 0, 66, 98, false, false},
  {"Highpass Soft Clip 4 Stage", 8, 0, 66, 99, false, false},
  {"Bandpass tanh 1 Stage", 12, 0, 64, 96, false, false},
  {"Bandpass tanh 2 Stage", 12, 0, 64, 97, false, false},
  {"Bandpass tanh 3 Stage", 12, 0, 64, 98, false, false},
  {"Bandpass tanh 4 Stage", 12, 0, 64, 99, false, false},
  {"Bandpass Soft Clip 1 Stage", 12, 0, 66, 96, false, false},
  {"Bandpass Soft Clip 2 Stage", 12, 0, 66, 97, false, false},
  {"Bandpass Soft Clip 3 Stage", 12, 0, 66, 98, false, false},
  {"Bandpass Soft Clip 4 Stage", 12, 0, 66, 99, false, false},
  {"Notch tanh 1 Stage", 16, 0, 64, 96, false, false},
  {"Notch tanh 2 Stage", 16, 0, 64, 97, false, false},
  {"Notch tanh 3 Stage", 16, 0, 64, 98, false, false},
  {"Notch tanh 4 Stage", 16, 0, 64, 99, false, false},
  {"Notch Soft Clip 1 Stage", 16, 0, 66, 96, false, false},
  {"Notch Soft Clip 2 Stage", 16, 0, 66, 97, false, false},
  {"Notch Soft Clip 3 Stage", 16, 0, 66, 98, false, false},
  {"Notch Soft Clip 4 Stage", 16, 0, 66, 99, false, false},
  {"Allpass tanh 1 Stage", 24, 0, 64, 96, false, false},
  {"Allpass tanh 2 Stage", 24, 0, 64, 97, false, false},
  {"Allpass tanh 3 Stage", 24, 0, 64, 98, false, false},
  {"Allpass tanh 4 Stage", 24, 0, 64, 99, false, false},
  {"Allpass Soft Clip 1 Stage", 24, 0, 66, 96, false, false},
  {"Allpass Soft Clip 2 Stage", 24, 0, 66, 97, false, false},
  {"Allpass Soft Clip 3 Stage", 24, 0, 66, 98, false, false},
  {"Allpass Soft Clip 4 Stage", 24, 0, 66, 99, false, false},
  // Fast SVF
  {"Lowpass", 4, 0, 0, 0, false, false},
  {"Highpass", 8, 0, 0, 0, false, false},
  {"Bandpass", 12, 0, 0, 0, false, false},
  {"Notch", 16, 0, 0, 0, false, false},
  {"Peak", 20, 0, 0, 0, false, false},
  {"Allpass", 24, 0, 0, 0, false, false},
  {"Low Shelf", 80, 0, 0, 0, true, true},
  {"Bell", 96, 0, 0, 0, true, true},
  {"High Shelf", 112, 0, 0, 0, true, true},
  // Tri-pole
  {"Low/Low/Low First", 48, 0, 0, 49, false, false},
  {"Low/Low/Low Second", 48, 0, 0, 50, false, false},
  {"Low/Low/Low Third", 48, 0, 0, 51, false, false},
  {"Low/High/Low First", 50, 0, 0, 49, false, false},
  {"Low/High/Low Second", 50, 0, 0, 50, false, false},
  {"Low/High/Low Third", 50, 0, 0, 51, false, false},
  {"High/Low/High First", 53, 0, 0, 49, false, false},
  {"High/Low/High Second", 53, 0, 0, 50, false, false},
  {"High/Low/High Third", 53, 0, 0, 51, false, false},
  {"High/High/High First", 55, 0, 0, 49, false, false},
  {"High/High/High Second", 55, 0, 0, 50, false, false},
  {"High/High/High Third", 55, 0, 0, 51, false, false},
  // Comb
  {"Negative Feedback, Variable Mix", 0, 80, 0, 0, true, false},
  {"Negative Feedback, Mix 100%", 0, 81, 0, 0, false, false},
  {"Negative Feedback, Mix 50%", 0, 82, 0, 0, false, false},
  {"Positive Feedback, Mix 50%", 0, 83, 0, 0, false, false},
  {"Positive Feedback, Mix 100%", 0, 84, 0, 0, false, false},
  {"Positive Feedback, Variable Mix", 0, 85, 0, 0, true, false},
  {"Bipolar, Variable Mix", 0, 86, 0, 0, true, true},
  // Sample & Hold
  {"", 0, 0, 0, 0, false, false},
  // Cascade SVF
  {"Lowpass 24 dB Standard", 4, 36, 16, 0, false, false},
  {"Lowpass 24 dB Standard Compensated", 4, 36, 16, 112, false, false},
  {"Lowpass 24 dB Driven", 4, 36, 20, 0, false, false},
  {"Lowpass 24 dB Driven Compensated", 4, 36, 20, 112, false, false},
  {"Highpass 24 dB Standard", 8, 36, 16, 0, false, false},
  {"Highpass 24 dB Driven", 8, 36, 20, 0, false, false},
  {"Bandpass 24 dB Standard", 12, 36, 16, 0, false, false},
  {"Bandpass 24 dB Driven", 12, 36, 20, 0, false, false},
  {"Notch 24 dB Standard", 16, 36, 16, 0, false, false},
  {"Notch 24 dB Driven", 16, 36, 20, 0, false, false},
  {"Peak 24 dB Standard", 20, 36, 16, 0, false, false},
  {"Peak 24 dB Driven", 20, 36, 20, 0, false, false},
  {"Allpass 24 dB Standard", 24, 36, 16, 0, false, false},
  {"Allpass 24 dB Driven", 24, 36, 20, 0, false, false},
}};
template <int M>
consteval auto variantNames()
{
  std::array<std::string_view, models[M].count> result{};
  for(int i = 0; i < models[M].count; ++i)
    result[i] = configurations[models[M].begin + i].name.empty()
                    ? "Standard" : configurations[models[M].begin + i].name;
  return result;
}
consteval auto modelNames()
{
  std::array<std::string_view, models.size()> result{};
  for(int i = 0; i < int(models.size()); ++i)
    result[i] = models[i].name;
  return result;
}
template <int M>
consteval auto extraVariants()
{
  constexpr auto count = [] {
    int n = 0;
    for(int i = 0; i < models[M].count; ++i)
      n += configurations[models[M].begin + i].extra;
    return n;
  }();
  std::array<int, count> result{};
  int n = 0;
  for(int i = 0; i < models[M].count; ++i)
    if(configurations[models[M].begin + i].extra)
      result[n++] = i;
  return result;
}
}
