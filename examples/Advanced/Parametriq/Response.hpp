#pragma once
#include "Model.hpp"
#include <array>
#include <cstdint>
#include <memory>

namespace examples::parametriq
{
inline constexpr int response_points = 512;
enum class ResponseStatus { Pending, Analytic, Reference, Unavailable, Failed };

// The parameter a handle drag shapes a band with, besides its gain: field_count
// when there is none.
Field shape_field(const Parameters&) noexcept;
double shape_value(const Parameters&, Field) noexcept;
// Positions of the shape parameter from 0 to 1, along the scale its drags and
// wheel use: Q is logarithmic, a band width logarithmic and reversed (up is
// narrower, like a higher Q), resonance linear.
double shape_position(const Parameters&, Field) noexcept;
double shape_at(Field, double position) noexcept;

// A band's own level at its frequency, in dB, at `level_points` positions of
// its shape parameter: what the curve shows there when that band shapes it
// alone. Denser towards the top, where resonances rise fastest.
inline constexpr int level_points = 13;
constexpr double level_position(int k) noexcept
{
  const double rest = 1. - double(k) / (level_points - 1);
  return 1. - rest * rest;
}
struct BandLevels
{
  bool valid{};
  // What they were measured for, with the shape parameter left out.
  Parameters band{};
  int quality{};
  double reference_db{}, rate{};
  std::array<double, level_points> db{};
};
// The band's own settings that its levels depend on.
Parameters level_key(Parameters) noexcept;
bool same_levels(const BandLevels&, const Parameters& key, const Settings&, double rate) noexcept;
// Analytic levels of a linear band; false for a nonlinear one, which only a
// reference measurement gives.
bool analytic_levels(const Settings&, int band, double rate, BandLevels&);

struct ResponseResult
{
  std::uint64_t revision{};
  ResponseStatus status = ResponseStatus::Pending;
  int unsettled{};
  std::array<double, response_points> db{};
  // Points drawn across measurements that did not settle: the linear bands'
  // exact response times the nonlinear ones' interpolated from either side.
  std::array<bool, response_points> estimated{};
  // The levels of the nonlinear bands measured along with this curve; the
  // others are not valid.
  std::array<BandLevels, band_count> levels{};
};

// GUI-owned handle; the shared worker never holds a widget or GUI callback.
// Destruction cancels work without joining a thread on the GUI thread.
// Analytic/unavailable requests complete synchronously; only nonlinear reference
// measurements use the worker. The GUI submits at its polling cadence.
// A reference measurement first gives a coarser curve, as a preview. While
// requests keep coming, previews are of earlier requests than the latest.
class ResponseService
{
public:
  ResponseService();
  ~ResponseService();
  ResponseService(ResponseService&&) noexcept;
  ResponseService& operator=(ResponseService&&) noexcept;
  ResponseService(const ResponseService&) = delete;
  ResponseService& operator=(const ResponseService&) = delete;

  std::uint64_t request(const Settings&, double rate);
  // The response of the latest request.
  bool take(ResponseResult&);
  // A reference curve to draw until then.
  bool take_preview(ResponseResult&);

private:
  struct State;
  struct Worker;
  std::shared_ptr<State> state;
  void cancel() noexcept;
};
} // namespace examples::parametriq
