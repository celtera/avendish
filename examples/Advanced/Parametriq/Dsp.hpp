#pragma once
#include "Model.hpp"

#include <memory>
#include <span>
#include <vector>

namespace examples::parametriq
{
// Histories, delay storage, kernels and coefficient designers stay out of the UI.
class Engine
{
public:
  Engine();
  ~Engine();
  Engine(Engine&&) noexcept;
  Engine& operator=(Engine&&) noexcept;
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  explicit operator bool() const noexcept { return bool(state); }

  // Comb delay lines, one per channel of each comb band. Only the comb model
  // uses them; they are allocated off the audio thread and handed over.
  struct Storage
  {
    std::vector<std::unique_ptr<float[]>> lines;
  };
  static Storage allocate(int lines);

  // Not real-time. A new sample rate or channel count builds a new state and
  // returns the previous one, so that the caller can destroy it elsewhere.
  // Comb storage is reserved for the given settings, twice when transitions
  // need a second copy of a comb band.
  Engine prepare(double rate, int channels, const Settings&, bool transitions = true);
  // Real-time: never allocates. A band switched to comb without free storage
  // keeps its previous configuration until adopt() provides some.
  void update(const Settings&) noexcept;
  int missing_storage() const noexcept;
  void adopt(Storage&) noexcept;
  void reset() noexcept;
  void process(const double* const* input, int input_channels,
               double* const* output, int output_channels, int frames) noexcept;

private:
  struct State;
  std::unique_ptr<State> state;
};

// Target response, including common gains and the shared resampler's images.
// False means a reference measurement (or no conventional response) is needed.
bool analytic_response(const Settings&, double rate,
                       std::span<const double> frequencies, std::span<double> db);
} // namespace examples::parametriq
