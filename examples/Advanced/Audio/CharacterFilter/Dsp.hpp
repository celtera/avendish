#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "Catalog.hpp"
#include <memory>

namespace ao::character_filter
{
struct Parameters
{
  int model = 0;
  int variant = 0;
  float cutoff = 1000.f;
  float resonance = 0.2f;
  float drive_db = 0.f;
  float output_db = 0.f;
  // 0..1, for the requested model only; converted to -1..1 for bipolar SST extras.
  float extra = 0.5f;
  // Off crossfades to the input passed through the same 2x oversampling as
  // the filter: bypass keeps the filter's delay, so toggling it neither combs
  // nor shifts the signal in time.
  bool enabled = true;
  bool operator==(const Parameters&) const = default;
};

// Whether the pinned SST accepts a catalog configuration. An unavailable
// configuration passes the driven input through unfiltered.
bool configurationAvailable(int configuration) noexcept;

// Lowest cutoff a configuration receives at a host rate. Higher than the
// 20 Hz control minimum only where SST's single-precision coefficient
// design degenerates at the internal rate.
float minimumCutoff(int configuration, double rate) noexcept;

// All SST/SIMD implementation details remain in the separately compiled adapter.
// Allocation is confined to prepare(). Independent histories per audio channel.
class Engine
{
public:
  Engine() noexcept;
  ~Engine();
  Engine(Engine&&) noexcept;
  Engine& operator=(Engine&&) noexcept;
  void prepare(double rate, int channels, Parameters parameters);
  void setParameters(Parameters parameters) noexcept;
  void process(const double* const* input, int input_channels,
               double* const* output, int output_channels, int frames) noexcept;

private:
  struct State;
  std::unique_ptr<State> state;
};
}
