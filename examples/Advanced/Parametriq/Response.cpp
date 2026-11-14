#include "Response.hpp"
#include "Dsp.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <complex>
#include <limits>
#include <deque>
#include <mutex>
#include <numbers>
#include <thread>
#include <vector>

namespace examples::parametriq
{
namespace
{
std::array<double, response_points> frequency_grid(double rate)
{
  std::array<double, response_points> frequencies;
  const double span = std::log(std::min(20000., rate * 0.49) / 20.);
  for(int i = 0; i < response_points; ++i)
    frequencies[i] = 20. * std::exp(span * i / (response_points - 1));
  return frequencies;
}
// A reference measurement measures one grid point out of `stride`, and the
// last one; the curve between them is interpolated in dB along the log grid.
constexpr int stride = 4;
constexpr int measured_points = (response_points - 2) / stride + 2;
constexpr int measured_index(int point) noexcept
{
  return std::min(point * stride, response_points - 1);
}
}

struct ResponseService::State
{
  std::mutex mutex;
  Settings settings;
  double rate = 48000.;
  std::chrono::steady_clock::time_point due;
  std::atomic<std::uint64_t> revision{};
  std::atomic<bool> alive{true};
  bool ready{};
  ResponseResult result;
  // The measurement being assembled from the parts the worker threads measure.
  std::array<double, measured_points> measured{};
  int parts_left{}, unsettled{};
  bool failed{};
};

// A few threads shared by every instance. Each measurement is split in parts,
// one per thread, so that it neither waits behind another instance's nor
// runs on a single core.
struct ResponseService::Worker
{
  struct Task
  {
    std::shared_ptr<State> state;
    std::uint64_t revision{};
    int part{};
  };
  std::mutex mutex;
  std::condition_variable wake;
  std::deque<Task> queue;
  bool stopping{};
  int parts{};
  std::vector<std::jthread> threads;

  Worker()
  {
    parts = std::clamp(int(std::thread::hardware_concurrency()) / 2, 1, 4);
    threads.reserve(parts);
    for(int i = 0; i < parts; ++i)
      threads.emplace_back([this](std::stop_token stop) { run(stop); });
  }
  ~Worker()
  {
    {
      std::lock_guard lock{mutex};
      stopping = true;
      for(auto& thread : threads)
        thread.request_stop();
    }
    wake.notify_all();
    // The jthreads join before the queue, wake condition and mutex are destroyed.
  }
  static Worker& instance()
  {
    static Worker worker;
    return worker;
  }
  // Called with the state's mutex held.
  void enqueue(const std::shared_ptr<State>& state, std::uint64_t revision)
  {
    state->parts_left = parts;
    state->unsettled = 0;
    state->failed = false;
    {
      std::lock_guard lock{mutex};
      // Parts of superseded measurements are cancelled: drop them unstarted.
      std::erase_if(queue, [&](const Task& t) { return t.state == state; });
      for(int part = 0; part < parts; ++part)
        queue.push_back({state, revision, part});
    }
    wake.notify_all();
  }

  // Fills the points of this part; false when cancelled.
  static bool measure(Engine& engine, const Settings& settings, double rate,
                      const std::array<double, response_points>& frequencies,
                      int part, int parts, std::array<double, measured_points>& db,
                      int& unsettled, bool& failed, const auto& cancelled)
  {
    const double amplitude = std::pow(10., settings.reference_db / 20.);
    engine.prepare(rate, 1, settings, false);
    constexpr int block = 128;
    std::array<double, block> input{}, output{};
    const double* inputs[]{input.data()};
    double* outputs[]{output.data()};
    for(int point = part; point < measured_points; point += parts)
    {
      if(cancelled())
        return false;
      engine.reset();
      const double frequency = frequencies[measured_index(point)];
      const double omega = 2. * std::numbers::pi * frequency / rate;
      const int settle = static_cast<int>(std::ceil(rate * std::max(0.06, 24. / frequency)));
      const int count = static_cast<int>(std::ceil(rate * std::max(0.02, 8. / frequency)));
      struct Fit
      {
        double ss{}, cc{}, sc{}, s{}, c{}, ys{}, yc{}, sum{};
        std::complex<double> fundamental(int count) const
        {
          const double a = ss - s * s / count, b = cc - c * c / count;
          const double cross = sc - s * c / count;
          const double u = ys - sum * s / count, v = yc - sum * c / count;
          const double determinant = a * b - cross * cross;
          return {(u * b - v * cross) / determinant,
                  (v * a - u * cross) / determinant};
        }
      };
      std::array<Fit, 3> fits{};
      for(int offset = 0; offset < settle + 3 * count;)
      {
        if(cancelled())
          return false;
        const int frames = std::min(block, settle + 3 * count - offset);
        for(int i = 0; i < frames; ++i)
          input[i] = amplitude * std::sin(omega * (offset + i));
        engine.process(inputs, 1, outputs, 1, frames);
        for(int i = 0; i < frames; ++i)
        {
          if(!std::isfinite(output[i]))
          {
            failed = true;
            return true;
          }
          if(offset + i < settle)
            continue;
          auto& fit = fits[(offset + i - settle) / count];
          const double sn = input[i] / amplitude;
          const double cs = std::cos(omega * (offset + i));
          fit.ss += sn * sn;
          fit.cc += cs * cs;
          fit.sc += sn * cs;
          fit.s += sn;
          fit.c += cs;
          fit.ys += output[i] * sn;
          fit.yc += output[i] * cs;
          fit.sum += output[i];
        }
        offset += frames;
      }
      // Fit sine, cosine and DC independently in three successive windows.
      // Autonomous oscillation and long transients have no stationary transfer:
      // omit those points instead of drawing window-dependent leakage as gain.
      const auto a = fits[0].fundamental(count);
      const auto b = fits[1].fundamental(count);
      const auto c = fits[2].fundamental(count);
      const double magnitude = std::abs((a + b + c) / 3.) / amplitude;
      if(!std::isfinite(magnitude))
      {
        failed = true;
        return true;
      }
      const double scale = std::max({std::abs(a), std::abs(b), std::abs(c), amplitude * 1.e-8});
      if(std::max({std::abs(a - b), std::abs(b - c), std::abs(a - c)}) > scale * 0.01)
      {
        ++unsettled;
        db[point] = std::numeric_limits<double>::quiet_NaN();
      }
      else
        db[point] = 20. * std::log10(std::max(magnitude, 1.e-12));
    }
    return true;
  }
  static void interpolate(const std::array<double, measured_points>& measured,
                          std::array<double, response_points>& db) noexcept
  {
    for(int point = 0; point + 1 < measured_points; ++point)
    {
      const int from = measured_index(point), to = measured_index(point + 1);
      for(int i = from; i <= to; ++i)
      {
        const double t = double(i - from) / (to - from);
        // NaN on either side stays NaN: unsettled regions are left out.
        db[i] = measured[point] + t * (measured[point + 1] - measured[point]);
      }
      db[from] = measured[point];
    }
    db.back() = measured.back();
  }

  void run(std::stop_token stop)
  {
    // Engine storage, comb lines included, lives only on this thread.
    std::unique_ptr<Engine> engine;
    for(;;)
    {
      Task task;
      {
        std::unique_lock lock{mutex};
        wake.wait(lock, [this] { return stopping || !queue.empty(); });
        if(stopping)
          return;
        task = std::move(queue.front());
        queue.pop_front();
      }
      auto& state = task.state;
      const auto revision = task.revision;
      const auto cancelled = [&] {
        return stop.stop_requested() || !state->alive || state->revision != revision;
      };
      if(cancelled())
        continue;
      Settings settings;
      double rate;
      std::chrono::steady_clock::time_point due;
      {
        std::lock_guard lock{state->mutex};
        settings = state->settings;
        rate = state->rate;
        due = state->due;
      }
      // A queued reference may have been superseded by a synchronous request.
      if(response_kind(settings) != ResponseKind::Reference)
        continue;
      // Only expensive nonlinear measurements wait for edits to settle.
      {
        std::unique_lock lock{mutex};
        wake.wait_until(lock, due, [&] { return stopping || cancelled(); });
      }
      if(cancelled())
        continue;
      std::array<double, measured_points> db{};
      int unsettled{};
      bool failed{};
      try
      {
        if(!engine)
          engine = std::make_unique<Engine>();
        const auto frequencies = frequency_grid(rate);
        if(!measure(*engine, settings, rate, frequencies, task.part, parts, db,
                    unsettled, failed, cancelled))
          continue;
      }
      catch(...)
      {
        failed = true;
      }
      std::lock_guard lock{state->mutex};
      if(!state->alive || state->revision != revision)
        continue;
      for(int point = task.part; point < measured_points; point += parts)
        state->measured[point] = db[point];
      state->unsettled += unsettled;
      state->failed |= failed;
      if(--state->parts_left == 0)
      {
        ResponseResult result;
        result.revision = revision;
        result.unsettled = state->unsettled;
        result.status = state->failed ? ResponseStatus::Failed : ResponseStatus::Reference;
        if(!state->failed)
          interpolate(state->measured, result.db);
        state->result = result;
        state->ready = true;
      }
    }
  }
};

ResponseService::ResponseService() : state{std::make_shared<State>()} { }
ResponseService::~ResponseService() { cancel(); }
ResponseService::ResponseService(ResponseService&&) noexcept = default;
ResponseService& ResponseService::operator=(ResponseService&& other) noexcept
{
  if(this != &other)
  {
    cancel();
    state = std::move(other.state);
  }
  return *this;
}
void ResponseService::cancel() noexcept
{
  if(state)
  {
    state->alive = false;
    ++state->revision;
  }
}
std::uint64_t ResponseService::request(const Settings& settings, double rate)
{
  const auto normalized = sanitize(settings, rate);
  rate = sample_rate(rate);
  const auto kind = response_kind(normalized);
  std::uint64_t revision;
  {
    std::lock_guard lock{state->mutex};
    state->settings = normalized;
    state->rate = rate;
    state->ready = false;
    state->due = std::chrono::steady_clock::now() + std::chrono::milliseconds{35};
    revision = ++state->revision;
    if(kind == ResponseKind::Reference)
      Worker::instance().enqueue(state, revision);
  }
  if(kind != ResponseKind::Reference)
  {
    // Called once per GUI polling tick with its latest settings. These bounded
    // analytic calculations must not wait for either input silence or the worker.
    ResponseResult result;
    result.revision = revision;
    result.status = ResponseStatus::Unavailable;
    if(kind == ResponseKind::Analytic)
    {
      result.status = ResponseStatus::Failed;
      try
      {
        const auto frequencies = frequency_grid(rate);
        if(analytic_response(normalized, rate, frequencies, result.db))
        {
          result.status = ResponseStatus::Analytic;
          for(double& value : result.db)
          {
            if(std::isnan(value) || value == INFINITY)
            {
              result.status = ResponseStatus::Failed;
              break;
            }
            value = std::max(value, -240.);
          }
        }
      }
      catch(...)
      {
        result.status = ResponseStatus::Failed;
      }
    }
    std::lock_guard lock{state->mutex};
    state->result = result;
    state->ready = true;
  }
  return revision;
}
bool ResponseService::take(ResponseResult& result)
{
  std::lock_guard lock{state->mutex};
  if(!state->ready)
    return false;
  result = state->result;
  state->ready = false;
  return true;
}
} // namespace examples::parametriq
