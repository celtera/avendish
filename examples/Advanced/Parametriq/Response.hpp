#pragma once
#include "Model.hpp"
#include <array>
#include <cstdint>
#include <memory>

namespace examples::parametriq
{
inline constexpr int response_points = 512;
enum class ResponseStatus { Pending, Analytic, Reference, Unavailable, Failed };
struct ResponseResult
{
  std::uint64_t revision{};
  ResponseStatus status = ResponseStatus::Pending;
  int unsettled{};
  std::array<double, response_points> db{};
};

// GUI-owned handle; the shared worker never holds a widget or GUI callback.
// Destruction cancels work without joining a thread on the GUI thread.
// Analytic/unavailable requests complete synchronously; only nonlinear reference
// measurements use the worker. The GUI submits at its polling cadence.
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
  bool take(ResponseResult&);

private:
  struct State;
  struct Worker;
  std::shared_ptr<State> state;
  void cancel() noexcept;
};
} // namespace examples::parametriq
