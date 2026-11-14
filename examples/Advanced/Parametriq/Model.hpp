#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace examples::parametriq
{
inline constexpr int band_count = 8;
// Contiguous IDs are also the native enum-control values.
enum Model : int { Butterworth, ChebyshevI, RBJ, Ivantsov, VemberClassic,
  VemberLadder, OBXd2, OBXd4, Xpander, K35, Diode, Vintage, CutoffWarp,
  ResonanceWarp, FastSVF, Tripole, Comb, SampleHold, CascadeSVF, model_count };
enum Response : int { LowPass, HighPass, BandPass, Notch, Peak, AllPass,
  LowShelf, Bell, HighShelf, Phaser, NotchLowPass, PhaserLowPass,
  LowLowLow, LowHighLow, HighLowHigh, HighHighHigh, Unspecified, response_count };
enum class ResponseKind { Analytic, Reference, Unavailable };
enum Field : int { Enabled, ModelField, ResponseField, Slope, Character,
  Submodel, Order, Frequency, Q, Bandwidth, Gain, ShelfSlope, Ripple,
  Resonance, Drive, Trim, Extra, Correction, Sigma, field_count };

template <Field F> consteval auto all_choice_names()
{
  if constexpr(F == ModelField)
    return std::array<std::string_view, model_count>{
      "Butterworth", "Chebyshev I", "RBJ", "Ivantsov", "Vember Classic",
      "Vember Ladder", "OB-Xd 2-pole", "OB-Xd 4-pole", "Xpander", "K35",
      "Diode Ladder", "Vintage Ladder", "Cutoff Warp", "Resonance Warp",
      "Fast SVF", "Tripole", "Comb", "Sample & Hold", "Cascade SVF"};
  else if constexpr(F == ResponseField)
    return std::array<std::string_view, response_count>{
      "Low-pass", "High-pass", "Band-pass", "Notch", "Peak", "All-pass",
      "Low shelf", "Bell", "High shelf", "Phaser", "Notch + low-pass",
      "Phaser + low-pass", "LP / LP / LP", "LP / HP / LP", "HP / LP / HP",
      "HP / HP / HP", "None"};
  else if constexpr(F == Slope)
    return std::array<std::string_view, 17>{
      "None", "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct",
      "6 / 12 dB/oct", "6 / 18 dB/oct", "12 / 6 dB/oct", "18 / 6 dB/oct",
      "Morph", "Negative / variable", "Negative / 100%", "Negative / 50%",
      "Positive / 50%", "Positive / 100%", "Positive / variable", "Bipolar / variable"};
  else if constexpr(F == Character)
    return std::array<std::string_view, 15>{
      "None", "Standard", "Clean", "Driven", "Mild notch", "Unsaturated",
      "Mild", "Moderate", "Heavy", "Extreme", "Continuous", "Pushed",
      "Tanh", "Soft clip", "OJD"};
  else if constexpr(F == Submodel)
    return std::array<std::string_view, 16>{
      "Default", "Original OB-Xd 24 dB", "Runge-Kutta", "Runge-Kutta compensated",
      "Huovilainen", "Huovilainen compensated", "Huovilainen 2010",
      "Huovilainen 2010 compensated", "First output", "Second output", "Third output",
      "1 stage", "2 stages", "3 stages", "4 stages", "Compensated"};
}

struct Parameters
{
  bool enabled = false;
  int model = RBJ;
  int response = Bell;
  int slope = 0;
  int character = 0;
  int submodel = 0;
  int order = 2;
  double frequency = 1000.;
  double q = 0.7071067811865476;
  double bandwidth = 1.; // octaves between the band's edges
  double gain = 0.;
  double shelf_slope = 1.;
  double ripple = 1.;
  double resonance = 0.2;
  double drive = 0.;
  double trim = 0.;
  double extra = 0.5;
  int correction = 2; // None, Omega, Sigma
  double sigma = 0.636619772367581343;
  bool operator==(const Parameters&) const = default;
};
struct Settings
{
  std::array<Parameters, band_count> bands{};
  int quality = 0; // 0 = 1x, 1 = 2x, 2 = 4x, shared by the whole chain
  double reference_db = -18.; // UI measurement only, never an audio gain
  bool operator==(const Settings&) const = default;
};
struct Choice { int value; std::string_view name; };
// Returns a small owning list: no allocation and no dangling dynamic spans.
struct Choices
{
  std::array<Choice, 32> entries{};
  int count{};
  auto begin() const { return entries.begin(); }
  auto end() const { return entries.begin() + count; }
  const Choice& operator[](int i) const { return entries[i]; }
};
std::span<const std::string_view, model_count> model_names() noexcept;
std::span<const std::string_view, response_count> response_names() noexcept;
// Categorical slope/character/submodel values are contiguous global label IDs;
// conversion to SST enum values is exclusively in the shared catalog.
Choices choices(const Parameters&, Field) noexcept;
Choices all_choices(Field) noexcept;
bool applicable(const Parameters&, Field) noexcept;
std::string_view label(const Parameters&, Field) noexcept;
ResponseKind response_kind(const Settings&) noexcept;
double sample_rate(double) noexcept;
Parameters sanitize(Parameters, double rate) noexcept;
// The edges of a band `octaves` wide around `frequency`, in Hz at `rate`: their
// ratio is 2^octaves and tan(pi f1 / rate) tan(pi f2 / rate) = tan^2(pi f / rate),
// so that the bilinear band transform puts its centre on the frequency.
struct BandEdges { double low, high; };
BandEdges band_edges(double frequency, double octaves, double rate) noexcept;
// The widest band at `frequency` whose upper edge stays a thousandth of the
// rate below Nyquist.
double max_bandwidth(double frequency, double rate) noexcept;
Settings sanitize(Settings, double rate) noexcept;
// Exact SST index in CharacterFilter::configurations, or -1 for linear models.
int character_configuration(const Parameters&) noexcept;
// Make a legal tuple, giving the user-edited categorical field precedence.
Parameters resolve(Parameters, Field changed = ModelField) noexcept;
} // namespace examples::parametriq
