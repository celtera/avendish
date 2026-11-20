#include "Response.hpp"
#include "Dsp.hpp"
#include <algorithm>
#include <atomic>
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
// A measurement runs in two passes: one point out of `coarse` first, which
// gives a curve to draw early, then the others.
constexpr int coarse = 4;
static_assert((measured_points - 1) % coarse == 0, "the last point is in the first pass");
// What a worker measures: a point of the curve, or one of a band's levels,
// numbered after the points.
constexpr int level_item(int band, int k) noexcept
{
  return measured_points + band * level_points + k;
}
// Calls f for the items of a part of a pass. The second pass also measures
// the levels of the bands in `levels`, which leaves the preview as early as it
// can be; handles keep the levels of an earlier measurement of the same filter
// meanwhile.
void for_each_item(int pass, int part, int parts,
                   const std::array<bool, band_count>& levels, const auto& f)
{
  int ordinal = 0;
  for(int point = 0; point < measured_points; ++point)
    if((point % coarse == 0) == (pass == 0) && ordinal++ % parts == part)
      f(point);
  if(pass != 1)
    return;
  for(int band = 0; band < band_count; ++band)
    if(levels[band])
      for(int k = 0; k < level_points; ++k)
        if(ordinal++ % parts == part)
          f(level_item(band, k));
}
bool analytic_band(const Parameters& p) noexcept
{
  return p.model < VemberClassic || p.model == FastSVF;
}
// The bands whose levels follow their shape parameter only through a measurement.
bool measured_levels(const Parameters& p) noexcept
{
  return p.enabled && !analytic_band(p) && p.model != SampleHold
         && !applicable(p, Gain) && shape_field(p) != field_count;
}
// The band alone, at the measured chain's quality and reference level.
Settings alone(const Settings& settings, const Parameters& p)
{
  Settings result;
  result.bands[0] = p;
  result.quality = settings.quality;
  result.reference_db = settings.reference_db;
  return result;
}
Parameters with_shape(Parameters p, double position) noexcept
{
  const Field field = shape_field(p);
  const double value = shape_at(field, position);
  switch(field)
  {
    case Q: p.q = value; break;
    case Bandwidth: p.bandwidth = value; break;
    case Resonance: p.resonance = value; break;
    default: break;
  }
  return p;
}

// Least-squares fit of the probe's sine and cosine and of DC in one window,
// with Blackman-Harris weights. Their sidelobes are 92 dB down: an autonomous
// oscillation or intermodulation products a few bins away from the probe, which
// a self-oscillating filter adds, do not leak into its estimate, so that the
// probe's transfer is found stationary beside them.
struct Fit
{
  double w{}, s{}, c{}, ss{}, cc{}, sc{}, ys{}, yc{}, y{};
  void add(double weight, double sn, double cs, double out) noexcept
  {
    w += weight;
    s += weight * sn;
    c += weight * cs;
    ss += weight * sn * sn;
    cc += weight * cs * cs;
    sc += weight * sn * cs;
    ys += weight * out * sn;
    yc += weight * out * cs;
    y += weight * out;
  }
  std::complex<double> fundamental() const noexcept
  {
    const double a = ss - s * s / w, b = cc - c * c / w;
    const double cross = sc - s * c / w;
    const double u = ys - y * s / w, v = yc - y * c / w;
    const double determinant = a * b - cross * cross;
    return {(u * b - v * cross) / determinant, (v * a - u * cross) / determinant};
  }
};
double blackman_harris(int k, int count) noexcept
{
  const double x = std::cos(2. * std::numbers::pi * k / count);
  // cos 2t and cos 3t from cos t.
  return 0.35875 - 0.48829 * x + 0.14128 * (2. * x * x - 1.)
         - 0.01168 * (4. * x * x - 3.) * x;
}
enum class Outcome { Settled, Unsettled, Failed, Cancelled };
// The gain of the engine for a sine at `frequency`, in dB.
Outcome measure_point(Engine& engine, double frequency, double amplitude, double rate,
                      double& db, const auto& cancelled)
{
  engine.reset();
  constexpr int block = 128;
  std::array<double, block> input{}, output{};
  const double* inputs[]{input.data()};
  double* outputs[]{output.data()};
  const double omega = 2. * std::numbers::pi * frequency / rate;
  const int settle = static_cast<int>(std::ceil(rate * std::max(0.06, 24. / frequency)));
  int count = static_cast<int>(std::ceil(rate * std::max(0.02, 8. / frequency)));
  int offset = 0;
  // Three successive windows must agree. When they do not, the signal goes on,
  // which settles long transients further, through three longer windows, which
  // separate an oscillation closer to the probe: only then is the point left out.
  for(int attempt = 0; attempt < 2; ++attempt)
  {
    const int skip = attempt == 0 ? settle : 0;
    const int length = skip + 3 * count;
    std::array<Fit, 3> fits{};
    for(int done = 0; done < length;)
    {
      if(cancelled())
        return Outcome::Cancelled;
      const int frames = std::min(block, length - done);
      for(int i = 0; i < frames; ++i)
        input[i] = amplitude * std::sin(omega * (offset + i));
      engine.process(inputs, 1, outputs, 1, frames);
      for(int i = 0; i < frames; ++i)
      {
        if(!std::isfinite(output[i]))
          return Outcome::Failed;
        const int k = done + i - skip;
        if(k < 0)
          continue;
        fits[k / count].add(blackman_harris(k % count, count), input[i] / amplitude,
                            std::cos(omega * (offset + i)), output[i]);
      }
      done += frames;
      offset += frames;
    }
    const auto a = fits[0].fundamental(), b = fits[1].fundamental(), c = fits[2].fundamental();
    const double magnitude = std::abs((a + b + c) / 3.) / amplitude;
    if(!std::isfinite(magnitude))
      return Outcome::Failed;
    // A beat with an oscillation too close to separate, or a transient that
    // outlasts the windows, has no stationary transfer: drawing what one
    // window caught as gain would be window-dependent leakage. Chaotic models
    // (the tri-pole) scatter a stationary transfer by about a percent from
    // window to window; 3%, a quarter of a dB, is still under a pixel.
    const double scale = std::max({std::abs(a), std::abs(b), std::abs(c), amplitude * 1.e-8});
    if(std::max({std::abs(a - b), std::abs(b - c), std::abs(a - c)}) <= scale * 0.03)
    {
      db = 20. * std::log10(std::max(magnitude, 1.e-12));
      return Outcome::Settled;
    }
    count = std::max(count, static_cast<int>(std::ceil(rate * 0.08)));
  }
  db = std::numeric_limits<double>::quiet_NaN();
  return Outcome::Unsettled;
}
// The response of the analytic bands alone, which a measured curve divides
// into theirs and the nonlinear bands' part.
std::array<double, response_points> linear_part(const Settings& settings, double rate)
{
  auto linear = settings;
  for(auto& p : linear.bands)
    if(!analytic_band(p))
      p.enabled = false;
  std::array<double, response_points> db{};
  const auto frequencies = frequency_grid(rate);
  if(!analytic_response(linear, rate, frequencies, db))
    db.fill(0.);
  for(double& value : db)
    if(!std::isfinite(value))
      value = 0.;
  return db;
}
}

Field shape_field(const Parameters& p) noexcept
{
  for(const Field field : {Q, Bandwidth, Resonance, ShelfSlope})
    if(applicable(p, field))
      return field;
  return field_count;
}
double shape_value(const Parameters& p, Field field) noexcept
{
  switch(field)
  {
    case Q: return p.q;
    case Bandwidth: return p.bandwidth;
    case Resonance: return p.resonance;
    case ShelfSlope: return p.shelf_slope;
    default: return 0.;
  }
}
namespace
{
// The ranges sanitize() keeps these parameters in; a band width's upper limit
// also depends on the frequency, which sanitize() applies after.
constexpr double q_low = 0.05, q_high = 50.;
constexpr double width_low = 0.0625, width_high = 8.;
}
double shape_position(const Parameters& p, Field field) noexcept
{
  const double value = shape_value(p, field);
  double position = 0.;
  switch(field)
  {
    case Q: position = std::log(value / q_low) / std::log(q_high / q_low); break;
    case Bandwidth: position = std::log(width_high / value) / std::log(width_high / width_low); break;
    case Resonance: position = value; break;
    case ShelfSlope: position = value; break;
    default: break;
  }
  return std::isfinite(position) ? std::clamp(position, 0., 1.) : 0.;
}
double shape_at(Field field, double position) noexcept
{
  position = std::clamp(position, 0., 1.);
  switch(field)
  {
    case Q: return q_low * std::pow(q_high / q_low, position);
    case Bandwidth: return width_high * std::pow(width_low / width_high, position);
    case Resonance: return position;
    case ShelfSlope: return position;
    default: return 0.;
  }
}
Parameters level_key(Parameters p) noexcept
{
  return with_shape(p, 0.);
}
bool same_levels(const BandLevels& levels, const Parameters& key, const Settings& s, double rate) noexcept
{
  return levels.valid && levels.band == key && levels.quality == s.quality
         && levels.reference_db == s.reference_db && levels.rate == rate;
}
bool analytic_levels(const Settings& settings, int band, double rate, BandLevels& levels)
{
  const auto& p = settings.bands[band];
  if(!p.enabled || !analytic_band(p))
    return false;
  const std::array<double, 1> frequency{p.frequency};
  for(int k = 0; k < level_points; ++k)
  {
    std::array<double, 1> db{};
    const auto s = sanitize(alone(settings, with_shape(p, level_position(k))), rate);
    if(!analytic_response(s, rate, frequency, db))
      return false;
    levels.db[k] = std::isnan(db[0]) ? -240. : std::clamp(db[0], -240., 240.);
  }
  levels.valid = true;
  levels.band = level_key(p);
  levels.quality = settings.quality;
  levels.reference_db = settings.reference_db;
  levels.rate = rate;
  return true;
}

struct ResponseService::State
{
  std::mutex mutex;
  // The latest request.
  Settings settings;
  double rate = 48000.;
  std::atomic<std::uint64_t> revision{};
  std::atomic<bool> alive{true};
  bool ready{};
  ResponseResult result;
  bool preview_ready{};
  ResponseResult preview;
  // The request being measured, 0 when none: while parameters keep changing
  // it is an earlier one than the latest.
  std::atomic<std::uint64_t> active{};
  Settings measured_settings;
  double measured_rate{};
  int pass{};
  // The measurement being assembled from the parts the worker threads measure.
  std::array<double, measured_points> measured{};
  // The bands whose levels this measurement measures: the others' are known.
  std::array<bool, band_count> measuring{};
  std::array<BandLevels, band_count> levels{};
  int parts_left{}, unsettled{};
  bool failed{};
};

// A few threads shared by every instance. Each pass of a measurement is split
// in parts, one per thread, so that it neither waits behind another instance's
// nor runs on a single core.
struct ResponseService::Worker
{
  struct Task
  {
    std::shared_ptr<State> state;
    std::uint64_t revision{};
    int pass{}, part{};
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
  // Both called with the state's mutex held.
  void start(const std::shared_ptr<State>& state)
  {
    state->measured_settings = state->settings;
    state->measured_rate = state->rate;
    state->active = state->revision.load();
    state->unsettled = 0;
    state->failed = false;
    // Levels do not depend on the shape parameter: a drag along it measures
    // only the curve.
    const auto& s = state->measured_settings;
    for(int band = 0; band < band_count; ++band)
    {
      const auto& p = s.bands[band];
      auto& levels = state->levels[band];
      state->measuring[band] = measured_levels(p)
          && !same_levels(levels, level_key(p), s, state->measured_rate);
      if(state->measuring[band])
        levels.valid = false;
    }
    begin_pass(state, 0);
  }
  void begin_pass(const std::shared_ptr<State>& state, int pass)
  {
    state->pass = pass;
    state->parts_left = parts;
    {
      std::lock_guard lock{mutex};
      // Parts of abandoned measurements are cancelled: drop them unstarted.
      std::erase_if(queue, [&](const Task& t) { return t.state == state; });
      for(int part = 0; part < parts; ++part)
        queue.push_back({state, state->active, pass, part});
    }
    wake.notify_all();
  }

  struct Part
  {
    std::array<double, measured_points> db{};
    std::array<std::array<double, level_points>, band_count> levels{};
    int unsettled{};
    bool failed{};
  };
  // Fills the items of this part of the pass; false when cancelled.
  static bool measure(Engine& engine, const Settings& settings, double rate,
                      const std::array<bool, band_count>& measuring,
                      int pass, int part, int parts, Part& result, const auto& cancelled)
  {
    const double amplitude = std::pow(10., settings.reference_db / 20.);
    const auto frequencies = frequency_grid(rate);
    // The engine is prepared for the chain (0), or for one of a band's levels.
    int prepared = -1;
    bool complete = true;
    for_each_item(pass, part, parts, measuring, [&](int item) {
      if(!complete || result.failed)
        return;
      double frequency;
      double* db;
      if(item < measured_points)
      {
        frequency = frequencies[measured_index(item)];
        db = &result.db[item];
        if(prepared != 0)
          engine.prepare(rate, 1, settings, false);
        prepared = 0;
      }
      else
      {
        const int band = (item - measured_points) / level_points;
        const int k = (item - measured_points) % level_points;
        const auto& p = settings.bands[band];
        frequency = p.frequency;
        db = &result.levels[band][k];
        engine.prepare(rate, 1, sanitize(alone(settings, with_shape(p, level_position(k))), rate), false);
        prepared = item;
      }
      switch(measure_point(engine, frequency, amplitude, rate, *db, cancelled))
      {
        case Outcome::Settled: break;
        case Outcome::Unsettled:
          if(item < measured_points)
            ++result.unsettled;
          break;
        case Outcome::Failed: result.failed = true; break;
        case Outcome::Cancelled: complete = false; break;
      }
    });
    return complete;
  }
  // The curve through one measured point out of `step`. The analytic bands'
  // exact response is kept apart: only the nonlinear bands' part is
  // interpolated, along the grid and across the points that did not settle,
  // from the settled ones on either side, or the nearest one at the ends.
  static void interpolate(const std::array<double, measured_points>& measured, int step,
                          const std::array<double, response_points>& linear,
                          ResponseResult& result) noexcept
  {
    constexpr int most = measured_points;
    std::array<double, most> rest{};
    std::array<bool, most> filled{};
    const int count = (measured_points - 1) / step + 1;
    int last = -1;
    for(int j = 0; j < count; ++j)
    {
      const double value = measured[j * step];
      if(!std::isfinite(value))
        continue;
      rest[j] = value - linear[measured_index(j * step)];
      for(int gap = last + 1; gap < j; ++gap)
      {
        rest[gap] = last < 0 ? rest[j]
            : rest[last] + (rest[j] - rest[last]) * double(gap - last) / (j - last);
        filled[gap] = true;
      }
      last = j;
    }
    if(last < 0)
    {
      result.db.fill(std::numeric_limits<double>::quiet_NaN());
      result.estimated.fill(false);
      return;
    }
    for(int gap = last + 1; gap < count; ++gap)
    {
      rest[gap] = rest[last];
      filled[gap] = true;
    }
    for(int j = 0; j + 1 < count; ++j)
    {
      const int from = measured_index(j * step), to = measured_index((j + 1) * step);
      for(int i = from; i <= to; ++i)
      {
        const double t = double(i - from) / (to - from);
        result.db[i] = linear[i] + rest[j] + t * (rest[j + 1] - rest[j]);
        result.estimated[i] = i == from ? filled[j] : i == to ? filled[j + 1]
                                                             : filled[j] || filled[j + 1];
      }
    }
  }
  // Called with the state's mutex held, once every part of a pass is measured.
  void finish_pass(const std::shared_ptr<State>& state)
  {
    const std::uint64_t revision = state->active;
    const auto& s = state->measured_settings;
    ResponseResult result;
    result.revision = revision;
    result.unsettled = state->unsettled;
    result.status = state->failed ? ResponseStatus::Failed : ResponseStatus::Reference;
    if(state->pass == 1 && !state->failed)
    {
      for(int band = 0; band < band_count; ++band)
        if(state->measuring[band])
          finish_levels(state->levels[band], s, band, state->measured_rate);
    }
    for(int band = 0; band < band_count; ++band)
      if(measured_levels(s.bands[band])
         && same_levels(state->levels[band], level_key(s.bands[band]), s, state->measured_rate))
        result.levels[band] = state->levels[band];
    std::array<double, response_points> linear{};
    if(!state->failed)
      linear = linear_part(s, state->measured_rate);
    if(state->pass == 0 && !state->failed)
    {
      interpolate(state->measured, coarse, linear, result);
      state->preview = result;
      state->preview_ready = true;
    }
    // The parameters changed meanwhile: this measurement stops at the curve
    // it gave, and the latest ones are measured.
    if(state->revision != revision)
      start(state);
    else if(state->pass == 0 && !state->failed)
      begin_pass(state, 1);
    else
    {
      if(!state->failed)
        interpolate(state->measured, 1, linear, result);
      state->result = result;
      state->ready = true;
      state->preview_ready = false;
      state->active = 0;
    }
  }
  // Levels that did not settle are interpolated from their neighbours, as the
  // curve is; none settled leaves the band without levels.
  static void finish_levels(BandLevels& levels, const Settings& s, int band, double rate) noexcept
  {
    int last = -1;
    for(int k = 0; k < level_points; ++k)
    {
      if(!std::isfinite(levels.db[k]))
        continue;
      for(int gap = last + 1; gap < k; ++gap)
        levels.db[gap] = last < 0 ? levels.db[k]
            : levels.db[last] + (levels.db[k] - levels.db[last])
                  * (level_position(gap) - level_position(last))
                  / (level_position(k) - level_position(last));
      last = k;
    }
    if(last < 0)
      return;
    for(int gap = last + 1; gap < level_points; ++gap)
      levels.db[gap] = levels.db[last];
    levels.valid = true;
    levels.band = level_key(s.bands[band]);
    levels.quality = s.quality;
    levels.reference_db = s.reference_db;
    levels.rate = rate;
  }

  void run(std::stop_token stop)
  {
    // Engine storage, comb lines included, lives only on this thread.
    std::unique_ptr<Engine> engine;
    std::unique_ptr<Part> part;
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
      const auto cancelled = [&] {
        return stop.stop_requested() || !state->alive || state->active != task.revision;
      };
      Settings settings;
      double rate;
      std::array<bool, band_count> measuring;
      {
        std::lock_guard lock{state->mutex};
        if(cancelled())
          continue;
        settings = state->measured_settings;
        rate = state->measured_rate;
        measuring = state->measuring;
      }
      try
      {
        if(!engine)
          engine = std::make_unique<Engine>();
        if(!part)
          part = std::make_unique<Part>();
        *part = Part{};
        if(!measure(*engine, settings, rate, measuring, task.pass, task.part, parts,
                    *part, cancelled))
          continue;
      }
      catch(...)
      {
        if(!part)
          continue;
        part->failed = true;
      }
      std::lock_guard lock{state->mutex};
      if(cancelled())
        continue;
      for_each_item(task.pass, task.part, parts, measuring, [&](int item) {
        if(item < measured_points)
          state->measured[item] = part->db[item];
        else
        {
          const int band = (item - measured_points) / level_points;
          const int k = (item - measured_points) % level_points;
          state->levels[band].db[k] = part->levels[band][k];
        }
      });
      state->unsettled += part->unsettled;
      state->failed |= part->failed;
      if(--state->parts_left == 0)
        finish_pass(state);
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
    revision = ++state->revision;
    if(kind != ResponseKind::Reference || rate != state->measured_rate)
    {
      state->active = 0;
      state->preview_ready = false;
    }
    // A measurement that did not give its first curve yet goes on to it while
    // parameters keep changing, then the latest ones are measured: restarting
    // on every change would draw nothing until the changes stop.
    if(kind == ResponseKind::Reference && (state->active == 0 || state->pass != 0))
      Worker::instance().start(state);
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
bool ResponseService::take_preview(ResponseResult& result)
{
  std::lock_guard lock{state->mutex};
  if(!state->preview_ready)
    return false;
  result = state->preview;
  state->preview_ready = false;
  return true;
}
} // namespace examples::parametriq
