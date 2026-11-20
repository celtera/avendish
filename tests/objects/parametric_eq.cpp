#include <catch2/catch_all.hpp>
#include <examples/Advanced/Parametriq/Dsp.hpp>
#include <examples/Advanced/Parametriq/EQDiagram.hpp>
#include <examples/Advanced/Parametriq/ParametricEq.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <limits>
#include <new>
#include <numbers>
#include <thread>
#include <vector>

// Counts the allocations made while `counting` is set, by any thread.
namespace
{
std::atomic<bool> counting{};
std::atomic<long> allocations{}, allocated_bytes{};
void* allocate_counted(std::size_t size)
{
  if(counting)
  {
    ++allocations;
    allocated_bytes += long(size);
  }
  if(void* p = std::malloc(size ? size : 1))
    return p;
  throw std::bad_alloc{};
}
struct Counter
{
  Counter() { allocations = 0; allocated_bytes = 0; counting = true; }
  ~Counter() { counting = false; }
};
}
void* operator new(std::size_t size) { return allocate_counted(size); }
void* operator new[](std::size_t size) { return allocate_counted(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace
{
namespace eq = examples::parametriq;
constexpr double rate = 48000.;
constexpr int block = 128;

eq::Settings setting(int model, int response, double frequency = 1000.)
{
  eq::Settings s;
  auto& p = s.bands[0];
  p.enabled = true;
  p.model = model;
  p.response = response;
  p.frequency = frequency;
  p.gain = 6.;
  p.bandwidth = 1.;
  p.ripple = 0.5;
  p = eq::resolve(p);
  return s;
}
void process(eq::Engine& e, const double* input, double* output, int count)
{
  const double* in[]{input};
  double* out[]{output};
  e.process(in, 1, out, 1, count);
}
double single(eq::Engine& e, double x)
{
  double y{};
  process(e, &x, &y, 1);
  return y;
}
double tone(eq::Engine& e, double frequency, double amplitude = 0.1, double fs = rate)
{
  std::array<double, block> input{}, output{};
  std::complex<double> projection{};
  const int settle = int(fs), measured = int(fs);
  for(int offset = 0; offset < settle + measured; offset += block)
  {
    const int count = std::min(block, settle + measured - offset);
    for(int i = 0; i < count; ++i)
      input[i] = amplitude * std::sin(2. * std::numbers::pi * frequency * (offset + i) / fs);
    process(e, input.data(), output.data(), count);
    for(int i = 0; i < count; ++i)
    {
      REQUIRE(std::isfinite(output[i]));
      if(offset + i >= settle)
        projection += output[i] * std::polar(1., -2. * std::numbers::pi * frequency * (offset + i) / fs);
    }
  }
  return 20. * std::log10(2. * std::abs(projection) / (measured * amplitude));
}
double analytic(const eq::Settings& s, double frequency, double fs = rate)
{
  const std::array<double, 1> frequencies{frequency};
  std::array<double, 1> db{};
  REQUIRE(eq::analytic_response(s, fs, frequencies, db));
  return db[0];
}
}

TEST_CASE("Parametriq independent bands compose rather than share histories", "[parametriq]")
{
  auto s = setting(eq::RBJ, eq::LowPass, 700.);
  s.bands[1] = s.bands[0];
  s.bands[1].frequency = 3200.;
  auto a = s; a.bands[1].enabled = false;
  auto b = s; b.bands[0].enabled = false;
  eq::Engine chain, first, second;
  chain.prepare(rate, 1, s); first.prepare(rate, 1, a); second.prepare(rate, 1, b);
  for(int i = 0; i < 4096; ++i)
  {
    const double x = i == 0 ? 1. : 0.;
    CHECK(single(chain, x) == Catch::Approx(single(second, single(first, x))).margin(1.e-13));
  }
}

TEST_CASE("Parametriq bypass and neutral shelves retain exact double samples", "[parametriq]")
{
  eq::Engine e;
  e.prepare(rate, 1, {});
  for(double x : {0., 0.123456789012345, 12., -8.}) CHECK(single(e, x) == x);
  for(int model : {eq::Butterworth, eq::ChebyshevI, eq::RBJ, eq::Ivantsov})
    for(int response : {eq::LowShelf, eq::Bell, eq::HighShelf})
    {
      auto s = setting(model, response);
      s.bands[0].gain = 0.;
      e.update(s); e.reset();
      for(double x : {0., 0.123456789012345, 12., -8.}) CHECK(single(e, x) == x);
    }
  e.update({}); e.reset();
  CHECK(single(e, std::numeric_limits<double>::quiet_NaN()) == 0.);
  CHECK(single(e, std::numeric_limits<double>::infinity()) == 0.);
  CHECK(single(e, -0.25) == -0.25);
}

TEST_CASE("Parametriq analytic graph follows actual linear chain and quality", "[parametriq]")
{
  eq::Engine e;
  e.prepare(rate, 1, {});
  for(int model : {eq::Butterworth, eq::ChebyshevI, eq::RBJ, eq::Ivantsov, eq::FastSVF})
    for(int response : {eq::LowPass, eq::Bell, eq::HighShelf})
      for(int quality = 0; quality < 3; ++quality)
      {
        INFO("model=" << model << " response=" << response << " quality=" << quality);
        auto s = setting(model, response, 15000.);
        s.quality = quality;
        s.bands[0].drive = -3.; s.bands[0].trim = 1.;
        s.bands[1] = eq::resolve(setting(eq::RBJ, eq::Bell, 1000.).bands[0]);
        s.bands[1].gain = -4.;
        for(double hz : {1000., 18000.})
        {
          INFO("tone=" << hz);
          e.update(s); e.reset();
          CHECK(tone(e, hz) == Catch::Approx(analytic(s, hz)).margin(0.003));
        }
      }
}

TEST_CASE("Parametriq resonant cuts expose real Q rather than fixed Butterworth damping", "[parametriq]")
{
  eq::Engine e;
  e.prepare(rate, 1, {});
  for(int model : {eq::RBJ, eq::Ivantsov})
    for(int response : {eq::LowPass, eq::HighPass})
      for(int correction : {1, 2})
        for(double q : {std::sqrt(0.5), 4.})
        {
          auto s = setting(model, response, 18000.);
          s.bands[0].q = q;
          s.bands[0].correction = correction;
          e.update(s); e.reset();
          INFO(model << " " << response << " correction=" << correction << " Q=" << q);
          CHECK(tone(e, 18000.) == Catch::Approx(20. * std::log10(q)).margin(0.0001));
        }
}

TEST_CASE("Parametriq Fast SVF bell gain is expressed in decibels", "[parametriq]")
{
  eq::Engine e;
  e.prepare(rate, 1, {});
  for(double gain : {-12., 0., 12.})
  {
    auto s = setting(eq::FastSVF, eq::Bell);
    s.bands[0].gain = gain;
    e.update(s); e.reset();
    CHECK(tone(e, 1000.) == Catch::Approx(gain).margin(0.01));
  }
}

TEST_CASE("Parametriq nonlinear reference level changes actual circuit response", "[parametriq]")
{
  auto s = setting(eq::K35, eq::LowPass);
  for(const auto& c : eq::choices(s.bands[0], eq::Character))
    if(c.name == "Continuous") s.bands[0].character = c.value;
  s.bands[0] = eq::resolve(s.bands[0], eq::Character);
  s.bands[0].resonance = 0.7;
  s.bands[0].drive = 12.;
  s.bands[0].extra = 0.8;
  s.quality = 1;
  eq::Engine e;
  e.prepare(rate, 1, s);
  const double quiet = tone(e, 1000., 0.001);
  e.reset();
  const double loud = tone(e, 1000., 0.5);
  CHECK(quiet - loud > 10.);
  // Reference level is a measurement control, not an audio gain or fade trigger.
  auto other = s; other.reference_db = -3.;
  eq::Engine reference;
  reference.prepare(rate, 1, s);
  e.reset();
  e.update(other);
  for(int i = 0; i < 1024; ++i)
  {
    const double x = 0.01 * std::sin(i * 0.1);
    CHECK(single(e, x) == single(reference, x));
  }
}

TEST_CASE("Parametriq rapid retargeting and bypass converge without repeated-fade starvation", "[parametriq]")
{
  auto s = setting(eq::RBJ, eq::LowShelf, 2000.);
  eq::Engine e;
  e.prepare(rate, 1, s);
  double previous{};
  for(int i = 0; i < 8192; ++i) previous = single(e, 0.125);
  s.bands[0].enabled = false;
  e.update(s);
  CHECK(std::abs(single(e, 0.125) - previous) < 0.002);
  s = setting(eq::Ivantsov, eq::HighPass, 4000.);
  s.quality = 2;
  for(int i = 0; i < 8192; ++i)
  {
    e.update(s);
    REQUIRE(std::isfinite(single(e, 0.125)));
  }
  CHECK(std::abs(single(e, 0.125)) < 1.e-7);
  s.bands[0].enabled = false; s.quality = 0;
  for(int i = 0; i < 8192; ++i) { e.update(s); single(e, 0.125); }
  CHECK(single(e, 0.125) == 0.125);
}

TEST_CASE("Parametriq channel layout changes never crossfeed histories", "[parametriq]")
{
  auto s = setting(eq::FastSVF, eq::LowPass, 1000.);
  s.quality = 2;
  eq::Engine e;
  std::array<std::array<double, block>, 5> input{}, output{};
  std::array<const double*, 5> in{};
  std::array<double*, 5> out{};
  for(int i = 0; i < 5; ++i) { in[i] = input[i].data(); out[i] = output[i].data(); }
  for(int channels : {1, 2, 5, 2, 0, 3})
  {
    e.prepare(rate, channels, s);
    e.reset();
    for(auto& channel : input) channel.fill(0.);
    if(channels) input[channels - 1][0] = 1.;
    e.process(in.data(), channels, out.data(), channels, 0);
    e.process(in.data(), channels, out.data(), channels, block);
    for(int c = 0; c + 1 < channels; ++c)
      for(double sample : output[c]) CHECK(sample == 0.);
    if(channels)
    {
      double energy{};
      for(double sample : output[channels - 1]) energy += sample * sample;
      CHECK(energy > 0.001);
      CHECK(energy < 1.);
    }
  }
}

namespace
{
struct Mouse
{
  double x{}, y{};
  int button = 1, modifiers = 0;
  static constexpr int left = 1, alt = 4;
};
struct GraphFixture
{
  eq::EQDiagram graph;
  int starts{}, commits{};
  template<std::size_t B> void assign(eq::Parameters p)
  {
    const auto fields = std::tuple{
      p.enabled, p.model, p.response, p.slope, p.character, p.submodel,
      p.order, p.frequency, p.q, p.bandwidth, p.gain, p.shelf_slope,
      p.ripple, p.resonance, p.drive, p.trim, p.extra, p.correction, p.sigma};
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((std::get<B * eq::field_count + I>(graph.values) = std::get<I>(fields)), ...);
    }(std::make_index_sequence<eq::field_count>{});
  }
  template<std::size_t I> void bind()
  {
    auto& t = std::get<I>(graph.transactions);
    t.start = [this] { ++starts; };
    t.commit = [this] { ++commits; };
    t.update = [this](auto value) { std::get<I>(graph.values) = value; };
  }
  GraphFixture()
  {
    [&]<std::size_t... B>(std::index_sequence<B...>) {
      (assign<B>({}), ...);
    }(std::make_index_sequence<eq::band_count>{});
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (bind<I>(), ...);
    }(std::make_index_sequence<std::tuple_size_v<eq::EQDiagram::Values>>{});
    std::get<eq::band_count * eq::field_count + 1>(graph.values) = -18.f;
  }
  // Where the band's handle is drawn.
  double handle_y(int band) const
  {
    return graph.y(graph.handle_level(band, eq::sanitize(graph.settings().bands[band], rate)));
  }
  // Polls until the response of the latest settings is there.
  void settle()
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while(graph.poll() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    REQUIRE(graph.status != eq::ResponseStatus::Pending);
  }
};
}

TEST_CASE("Parametriq graph creates resonant cuts at exact edge thresholds", "[parametriq][ui]")
{
  for(const auto [frequency, response] : std::array<std::pair<double, int>, 6>{{
        {20., eq::HighPass}, {50., eq::HighPass}, {50.001, eq::Bell},
        {9999.999, eq::Bell}, {10000., eq::LowPass}, {20000., eq::LowPass}}})
  {
    GraphFixture f;
    REQUIRE(f.graph.mouse_double_click(Mouse{f.graph.x(frequency), f.graph.y(6.)}));
    const auto p = f.graph.settings().bands[0];
    CHECK(p.enabled);
    CHECK(p.model == eq::RBJ);
    CHECK(p.response == response);
    CHECK(p.frequency == Catch::Approx(frequency).epsilon(1.e-6));
    CHECK(p.gain == (response == eq::Bell ? 6. : 0.));
    CHECK(f.starts == 1);
    CHECK(f.commits == 1);
  }
}

TEST_CASE("Parametriq graph selection is not an edit and full banks are preserved", "[parametriq][ui]")
{
  GraphFixture f;
  for(int i = 0; i < eq::band_count; ++i)
  {
    eq::EQDiagram::visit_band(i, [&]<std::size_t B> {
      f.assign<B>(setting(eq::RBJ, eq::Bell, 100. * std::pow(1.9, i)).bands[0]);
    });
  }
  const auto before = f.graph.values;
  f.graph.mouse_double_click(Mouse{f.graph.x(15000.), f.graph.y(-15.)});
  CHECK(f.graph.values == before);
  CHECK(f.starts == 0);
  const Mouse position{f.graph.x(100.), f.graph.y(6.)};
  REQUIRE(f.graph.mouse_press(position));
  f.graph.mouse_release(position);
  CHECK(f.graph.selected == 0);
  CHECK(f.commits == 0);
  CHECK_FALSE(f.graph.wheel(f.graph.left + 10., f.graph.top + 10., 120.));
}

TEST_CASE("Parametriq graph edits only the active model shape parameter", "[parametriq][ui]")
{
  for(int model : {eq::RBJ, eq::Butterworth, eq::Ivantsov, eq::K35})
  {
    GraphFixture f;
    auto p = setting(model, model == eq::Butterworth ? eq::BandPass : eq::LowPass).bands[0];
    f.assign<0>(p);
    const auto before = f.graph.settings().bands[0];
    const double px = f.graph.x(p.frequency);
    REQUIRE(f.graph.wheel(px, f.handle_y(0), 120.));
    auto after = f.graph.settings().bands[0];
    if(model == eq::Butterworth)
    {
      CHECK(after.bandwidth < before.bandwidth);
      CHECK(after.q == before.q);
    }
    else if(model == eq::K35)
    {
      CHECK(after.resonance > before.resonance);
      CHECK(after.q == before.q);
    }
    else
    {
      CHECK(after.q > before.q);
      CHECK(after.bandwidth == before.bandwidth);
    }
    // These bands have no gain: dragging their handle up shapes them instead.
    const auto wheeled = after;
    double py = f.handle_y(0);
    REQUIRE(f.graph.mouse_press(Mouse{px, py}));
    f.graph.mouse_move(Mouse{f.graph.x(2000.), py - 28.});
    f.graph.mouse_release(Mouse{f.graph.x(2000.), py - 28.});
    after = f.graph.settings().bands[0];
    CHECK(after.frequency == Catch::Approx(2000.).epsilon(1.e-6));
    CHECK(after.gain == wheeled.gain);
    if(model == eq::Butterworth)
    {
      CHECK(after.bandwidth < wheeled.bandwidth);
      CHECK(after.q == wheeled.q);
    }
    else if(model == eq::K35)
    {
      CHECK(after.resonance > wheeled.resonance);
      CHECK(after.q == wheeled.q);
    }
    else
    {
      CHECK(after.q > wheeled.q);
      CHECK(after.bandwidth == wheeled.bandwidth);
    }
    CHECK(f.starts == 2);
    CHECK(f.commits == 2);

    // Down goes the other way, and a horizontal drag leaves the shape alone.
    const auto raised = after;
    const double at = f.graph.x(after.frequency);
    py = f.handle_y(0);
    REQUIRE(f.graph.mouse_press(Mouse{at, py}));
    f.graph.mouse_move(Mouse{at, py + 14.});
    f.graph.mouse_release(Mouse{at, py + 14.});
    after = f.graph.settings().bands[0];
    CHECK(after.frequency == Catch::Approx(raised.frequency).epsilon(1.e-6));
    if(model == eq::Butterworth)
      CHECK(after.bandwidth > raised.bandwidth);
    else if(model == eq::K35)
      CHECK(after.resonance < raised.resonance);
    else
      CHECK(after.q < raised.q);
    const auto lowered = after;
    py = f.handle_y(0);
    REQUIRE(f.graph.mouse_press(Mouse{at, py}));
    f.graph.mouse_move(Mouse{f.graph.x(500.), py});
    f.graph.mouse_release(Mouse{f.graph.x(500.), py});
    after = f.graph.settings().bands[0];
    CHECK(after.frequency == Catch::Approx(500.).epsilon(1.e-6));
    CHECK(after.q == lowered.q);
    CHECK(after.resonance == lowered.resonance);
    if(model != eq::Butterworth)
      CHECK(after.bandwidth == lowered.bandwidth);
  }
}

TEST_CASE("Parametriq graph drags the gain of a band that has one, not its shape", "[parametriq][ui]")
{
  for(int response : {eq::Bell, eq::LowShelf, eq::HighShelf})
  {
    GraphFixture f;
    const auto p = setting(eq::RBJ, response).bands[0];
    f.assign<0>(p);
    const auto before = f.graph.settings().bands[0];
    REQUIRE(f.graph.mouse_press(Mouse{f.graph.x(p.frequency), f.graph.y(before.gain)}));
    f.graph.mouse_move(Mouse{f.graph.x(p.frequency), f.graph.y(9.)});
    f.graph.mouse_release(Mouse{f.graph.x(p.frequency), f.graph.y(9.)});
    const auto after = f.graph.settings().bands[0];
    CHECK(after.gain == Catch::Approx(9.));
    CHECK(after.q == before.q);
    CHECK(after.shelf_slope == before.shelf_slope);
    CHECK(after.bandwidth == before.bandwidth);
  }
}

TEST_CASE("Parametriq wheel edits share a gesture until its target or model changes", "[parametriq][ui]")
{
  GraphFixture f;
  auto p = setting(eq::RBJ, eq::LowPass).bands[0];
  f.assign<0>(p);
  const double px = f.graph.x(p.frequency), py = f.handle_y(0);
  REQUIRE(f.graph.wheel(px, py, 120.));
  REQUIRE(f.graph.wheel(px, py, 120.));
  REQUIRE(f.graph.wheel(px, py, -60.)); // Reversing direction is the same adjustment.
  CHECK(f.graph.settings().bands[0].q == Catch::Approx(p.q * std::exp2(180. / 480.)));
  CHECK(f.starts == 1);
  CHECK(f.commits == 0);

  p = f.graph.settings().bands[0];
  p.model = eq::Ivantsov; // Still Q, but a different filter configuration.
  f.assign<0>(p);
  f.graph.refresh();
  CHECK(f.commits == 1);
  REQUIRE(f.graph.wheel(px, py, 120.));
  CHECK(f.starts == 2);
  f.graph.on_selection_changed(1);
  CHECK(f.commits == 2);
  f.graph.finish_wheel();
  CHECK(f.commits == 2);

  p.q = 50.;
  f.assign<0>(p);
  REQUIRE(f.graph.wheel(px, f.handle_y(0), 120.));
  f.graph.finish_wheel();
  CHECK(f.graph.settings().bands[0].q == 50.);
  CHECK(f.starts == 2);
  CHECK(f.commits == 2);
}

TEST_CASE("Parametriq curve follows a sustained drag without waiting for release", "[parametriq][ui]")
{
  GraphFixture f;
  f.assign<0>(setting(eq::RBJ, eq::Bell).bands[0]);
  REQUIRE_FALSE(f.graph.poll());
  REQUIRE(f.graph.has_curve);
  REQUIRE(f.graph.mouse_press(Mouse{f.graph.x(1000.), f.graph.y(6.)}));
  for(const auto [frequency, gain] : std::array<std::pair<double, double>, 3>{{
          {2000., 6.}, {4000., 12.}, {800., 9.}}})
  {
    const auto previous = f.graph.ordinate;
    f.graph.mouse_move(Mouse{f.graph.x(frequency), f.graph.y(gain)});
    f.graph.refresh();
    REQUIRE(f.graph.has_curve);
    CHECK(f.graph.ordinate == previous); // Keep drawing between display ticks.
    REQUIRE_FALSE(f.graph.poll()); // A tick uses the latest position, without debounce.
    const auto peak = std::min_element(f.graph.ordinate.begin(), f.graph.ordinate.end());
    const double px = f.graph.left + f.graph.plot_width
        * std::distance(f.graph.ordinate.begin(), peak) / (eq::response_points - 1);
    CHECK(px == Catch::Approx(f.graph.x(frequency)).margin(1.));
    CHECK(*peak == Catch::Approx(f.graph.y(gain)).margin(0.05));
    CHECK(f.graph.dragging);
  }
  f.graph.mouse_release(Mouse{f.graph.x(800.), f.graph.y(9.)});
  CHECK(f.commits == 1);
}

TEST_CASE("Parametriq retains a curve during reference work but clears unavailable responses", "[parametriq][ui]")
{
  GraphFixture f;
  f.assign<0>(setting(eq::RBJ, eq::Bell).bands[0]);
  REQUIRE_FALSE(f.graph.poll());
  const auto previous = f.graph.ordinate;
  f.assign<0>(setting(eq::K35, eq::LowPass).bands[0]);
  REQUIRE(f.graph.poll());
  CHECK(f.graph.has_curve);
  CHECK(f.graph.ordinate == previous);
  f.assign<0>(setting(eq::SampleHold, eq::Unspecified).bands[0]);
  f.graph.refresh();
  CHECK_FALSE(f.graph.has_curve);
  REQUIRE_FALSE(f.graph.poll());
  CHECK(f.graph.status == eq::ResponseStatus::Unavailable);
  // Switching away from a queued reference must not hold up the new analytic curve.
  f.assign<0>(setting(eq::RBJ, eq::HighPass).bands[0]);
  REQUIRE_FALSE(f.graph.poll());
  CHECK(f.graph.has_curve);
  CHECK(f.graph.ordinate.front() > f.graph.ordinate.back());
}

TEST_CASE("Parametriq reference measurements give a preview, then the same curve as alone", "[parametriq]")
{
  auto s = setting(eq::K35, eq::LowPass);
  const auto wait = [](eq::ResponseService& service, eq::ResponseResult& result, int& previews) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while(std::chrono::steady_clock::now() < deadline)
    {
      eq::ResponseResult preview;
      if(service.take_preview(preview))
      {
        ++previews;
        CHECK(preview.status == eq::ResponseStatus::Reference);
      }
      if(service.take(result))
        return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
  };
  eq::ResponseService alone;
  eq::ResponseResult expected;
  int previews = 0;
  const auto first = alone.request(s, rate);
  REQUIRE(wait(alone, expected, previews));
  REQUIRE(expected.revision == first);
  REQUIRE(expected.status == eq::ResponseStatus::Reference);
  CHECK(previews <= 1);
  // Its response once taken, a measurement has no preview left to draw over it.
  eq::ResponseResult none;
  CHECK_FALSE(alone.take_preview(none));

  // Parameters that keep changing, as in a drag: curves keep coming meanwhile,
  // each of a later request than the one before.
  eq::ResponseService dragged;
  std::uint64_t last{}, shown{};
  previews = 0;
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  for(int i = 0; previews < 3 && std::chrono::steady_clock::now() < until; ++i)
  {
    auto moving = s;
    moving.bands[0].frequency = 200. + 10. * (i % 300);
    last = dragged.request(moving, rate);
    eq::ResponseResult preview;
    if(dragged.take_preview(preview))
    {
      ++previews;
      CHECK(preview.revision > shown);
      CHECK(preview.revision <= last);
      shown = preview.revision;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(previews == 3);
  // Then the response of the last request is the one a measurement alone gives.
  last = dragged.request(s, rate);
  eq::ResponseResult result;
  REQUIRE(wait(dragged, result, previews));
  CHECK(result.revision == last);
  REQUIRE(result.status == eq::ResponseStatus::Reference);
  CHECK(result.unsettled == expected.unsettled);
  for(int i = 0; i < eq::response_points; ++i)
    CHECK((result.db[i] == expected.db[i]
           || (std::isnan(result.db[i]) && std::isnan(expected.db[i]))));
}

TEST_CASE("Parametriq draws reference previews until the response, never over an analytic curve", "[parametriq][ui]")
{
  GraphFixture f;
  f.assign<0>(setting(eq::RBJ, eq::Bell).bands[0]);
  REQUIRE_FALSE(f.graph.poll());
  const auto analytic_curve = f.graph.ordinate;
  f.assign<0>(setting(eq::K35, eq::LowPass).bands[0]);
  int curves = 0;
  auto previous = f.graph.ordinate;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while(f.graph.poll() && std::chrono::steady_clock::now() < deadline)
  {
    CHECK(f.graph.has_curve);
    if(f.graph.ordinate != previous)
    {
      ++curves;
      CHECK(f.graph.status == eq::ResponseStatus::Pending);
      previous = f.graph.ordinate;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(f.graph.status == eq::ResponseStatus::Reference);
  CHECK(curves <= 1);
  CHECK(f.graph.ordinate != analytic_curve);

  // A reference measurement under way gives nothing once the band is analytic.
  f.assign<0>(setting(eq::K35, eq::HighPass).bands[0]);
  REQUIRE(f.graph.poll());
  f.assign<0>(setting(eq::RBJ, eq::Bell).bands[0]);
  REQUIRE_FALSE(f.graph.poll());
  CHECK(f.graph.ordinate == analytic_curve);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  REQUIRE_FALSE(f.graph.poll());
  CHECK(f.graph.status == eq::ResponseStatus::Analytic);
  CHECK(f.graph.ordinate == analytic_curve);
}

TEST_CASE("Parametriq repeated prepare preserves sounding histories", "[parametriq]")
{
  const auto settings = setting(eq::Ivantsov, eq::LowPass, 250.);
  eq::Engine uninterrupted, prepared;
  uninterrupted.prepare(rate, 1, settings);
  prepared.prepare(rate, 1, settings);
  for(int i = 0; i < 8192; ++i)
  {
    if(i % 128 == 0) prepared.prepare(rate, 1, settings);
    const double input = std::sin(i * 0.019);
    CHECK(single(prepared, input) == single(uninterrupted, input));
  }
}

TEST_CASE("Parametriq draws nonstationary circuit leakage as an estimate, never as gain", "[parametriq]")
{
  auto s = setting(eq::K35, eq::LowPass);
  for(const auto& c : eq::choices(s.bands[0], eq::Character))
    if(c.name == "Continuous") s.bands[0].character = c.value;
  s.bands[0] = eq::resolve(s.bands[0], eq::Character);
  s.bands[0].resonance = 0.7;
  s.bands[0].drive = 12.;
  s.bands[0].extra = 0.8;
  s.quality = 1;
  s.reference_db = -60.;
  eq::ResponseService service;
  const auto revision = service.request(s, rate);
  eq::ResponseResult result;
  bool ready = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while(std::chrono::steady_clock::now() < deadline)
  {
    if(service.take(result)) { ready = true; break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(ready);
  REQUIRE(result.revision == revision);
  REQUIRE(result.status == eq::ResponseStatus::Reference);
  const int near_cutoff = int(std::lround(std::log(50.) / std::log(1000.) * (eq::response_points - 1)));
  CHECK(result.unsettled > 0);
  // Where windows disagree, points are drawn dashed, between the settled
  // ones around them, not at what one window caught.
  int estimated = 0;
  for(int i = 0; i < eq::response_points; ++i)
  {
    REQUIRE(std::isfinite(result.db[i]));
    if(!result.estimated[i])
      continue;
    ++estimated;
    int below = i, above = i;
    while(below > 0 && result.estimated[below]) --below;
    while(above + 1 < eq::response_points && result.estimated[above]) ++above;
    CHECK(result.db[i] >= std::min(result.db[below], result.db[above]) - 0.1);
    CHECK(result.db[i] <= std::max(result.db[below], result.db[above]) + 0.1);
  }
  CHECK(estimated > 0);
  // Its peak is measured as a second-long run gives it.
  eq::Engine e;
  e.prepare(rate, 1, s);
  CHECK_FALSE(result.estimated[near_cutoff]);
  CHECK(result.db[near_cutoff] == Catch::Approx(tone(e, 1000., std::pow(10., s.reference_db / 20.))).margin(0.5));
}

TEST_CASE("Parametriq editing one band preserves other resonant histories", "[parametriq]")
{
  auto s = setting(eq::Ivantsov, eq::LowPass);
  s.bands[0].q = 50.;
  // A bell that changes the signal: editing its trim must not restart it either.
  s.bands[7] = setting(eq::RBJ, eq::Bell, 3000.).bands[0];
  s.bands[7].q = 4.;
  eq::Engine reference, edited;
  reference.prepare(rate, 1, s);
  edited.prepare(rate, 1, s);
  for(int i = 0; i < 8192; ++i)
  {
    const double x = 0.1 * std::sin(2. * std::numbers::pi * 1000. * i / rate);
    single(reference, x);
    single(edited, x);
  }
  s.bands[7].trim = 6.;
  edited.update(s);
  const double gain = std::pow(10., 6. / 20.);
  double previous = 1., worst{};
  for(int i = 0; i < 2048; ++i)
  {
    const double x = 0.1 * std::sin(2. * std::numbers::pi * 1000. * (i + 8192) / rate);
    const double expected = single(reference, x), actual = single(edited, x);
    if(std::abs(expected) > 1.e-3)
    {
      // The trim moves from 1 to its target and stays there.
      const double ratio = actual / expected;
      CHECK(ratio >= previous - 1.e-9);
      CHECK(ratio <= gain + 1.e-9);
      previous = ratio;
    }
    if(i >= 1024)
      worst = std::max(worst, std::abs(actual - gain * expected));
  }
  CHECK(worst < 1.e-12);
}

namespace
{
// Peak of the band's output over 20 ms windows while one of its parameters
// alternates between two close values on every 64-frame block, relative to
// its peak when nothing moves, in dB. Restarting the band at each change
// would make it fall.
double automation_drop(eq::Settings s, eq::Field field, double tone_hz)
{
  constexpr int frames = 64;
  eq::Engine e;
  e.prepare(rate, 1, s);
  std::array<double, frames> input{}, output{};
  long n{};
  const auto run = [&] {
    for(auto& x : input)
      x = 0.1 * std::sin(2. * std::numbers::pi * tone_hz * double(n++) / rate);
    process(e, input.data(), output.data(), frames);
  };
  double steady{};
  for(int b = 0; b < int(4 * rate / frames); ++b)
  {
    run();
    if(b > int(3 * rate / frames))
      for(double v : output)
        steady = std::max(steady, std::abs(v));
  }
  const auto base = s.bands[0];
  auto& p = s.bands[0];
  double lowest = std::numeric_limits<double>::infinity(), window{};
  const int per_window = int(0.02 * rate / frames);
  for(int b = 0; b < int(rate / frames); ++b)
  {
    p = base;
    if(b & 1)
    {
      switch(field)
      {
        case eq::Frequency: p.frequency *= 1.01; break;
        case eq::Q: p.q *= 1.01; break;
        case eq::Bandwidth: p.bandwidth *= 1.01; break;
        case eq::Gain: p.gain += 0.1; break;
        case eq::ShelfSlope: p.shelf_slope -= 0.01; break;
        case eq::Ripple: p.ripple += 0.01; break;
        case eq::Resonance: p.resonance += 0.01; break;
        case eq::Drive: p.drive += 0.01; break;
        case eq::Trim: p.trim += 0.01; break;
        case eq::Extra: p.extra += 0.01; break;
        case eq::Sigma: p.sigma += 0.01; break;
        default: break;
      }
    }
    e.update(s);
    run();
    for(double v : output)
    {
      REQUIRE(std::isfinite(v));
      window = std::max(window, std::abs(v));
    }
    if((b + 1) % per_window == 0)
    {
      lowest = std::min(lowest, window);
      window = 0.;
    }
  }
  return 20. * std::log10(lowest / steady);
}
}

TEST_CASE("Parametriq automating a continuous parameter keeps its band sounding", "[parametriq]")
{
  struct Case { int model, response; double frequency, tone; };
  for(const auto c : std::array<Case, 12>{{
        {eq::RBJ, eq::Bell, 60., 60.}, {eq::RBJ, eq::LowShelf, 200., 60.},
        {eq::Butterworth, eq::Bell, 200., 200.}, {eq::Butterworth, eq::LowPass, 100., 50.},
        {eq::ChebyshevI, eq::Bell, 200., 200.}, {eq::ChebyshevI, eq::LowPass, 100., 50.},
        {eq::Ivantsov, eq::Bell, 200., 200.}, {eq::Ivantsov, eq::LowPass, 100., 50.},
        {eq::K35, eq::LowPass, 400., 200.}, {eq::OBXd4, eq::LowPass, 400., 200.},
        {eq::FastSVF, eq::Bell, 400., 400.}, {eq::Comb, eq::Unspecified, 400., 200.}}})
  {
    auto s = setting(c.model, c.response, c.frequency);
    auto& p = s.bands[0];
    p.order = 4;
    p.q = 8.;
    p.gain = 12.;
    p.ripple = 1.;
    p.bandwidth = 0.7;
    p.resonance = 0.5;
    p.shelf_slope = 0.5;
    p = eq::sanitize(eq::resolve(p, eq::ResponseField), rate);
    for(int f = eq::Frequency; f < eq::field_count; ++f)
    {
      const auto field = eq::Field(f);
      if(!eq::applicable(p, field) || field == eq::Correction)
        continue;
      INFO("model=" << c.model << " response=" << p.response << " field=" << f);
      CHECK(automation_drop(s, field, c.tone) > -0.3);
    }
  }
}

TEST_CASE("Parametriq processing, retuning and reconfiguration never allocate", "[parametriq]")
{
  constexpr int frames = 64;
  std::array<std::array<double, frames>, 2> input{}, output{}, other{};
  for(auto& channel : input)
    for(int i = 0; i < frames; ++i)
      channel[i] = 0.25 * std::sin(i * 0.3);
  const double* in[]{input[0].data(), input[1].data()};
  double* out[]{output[0].data(), output[1].data()};
  double* out_other[]{other[0].data(), other[1].data()};

  auto s = setting(eq::RBJ, eq::Bell);
  s.bands[1] = setting(eq::Comb, eq::Unspecified, 300.).bands[0];
  std::vector<eq::Settings> steps;
  for(double f : {1100., 1200., 900.})
  {
    s.bands[0].frequency = f;
    s.bands[1].frequency = f / 3.;
    s.bands[1].resonance = f / 2000.;
    steps.push_back(s);
  }
  for(int model : {eq::K35, eq::Ivantsov, eq::Butterworth, eq::FastSVF, eq::OBXd4})
  {
    s.bands[0] = setting(model, eq::LowPass, 2000.).bands[0];
    steps.push_back(s);
  }
  s.quality = 2;
  steps.push_back(s);
  // Another comb configuration fades from the first: the second copy prepare reserved.
  s.bands[1].slope = eq::choices(s.bands[1], eq::Slope)[1].value;
  s.bands[1] = eq::resolve(s.bands[1], eq::Slope);
  steps.push_back(s);
  s.quality = 0;
  steps.push_back(s);

  eq::Engine e;
  e.prepare(rate, 2, steps.front());
  {
    Counter counter;
    for(const auto& step : steps)
      for(int block = 0; block < 16; ++block)
      {
        e.update(step);
        e.process(in, 2, out, 2, frames);
      }
  }
  CHECK(allocations == 0);
  CHECK(e.missing_storage() == 0);

  // A band switched to comb without storage keeps its configuration, here
  // disabled, until storage comes: until then it sounds like the reference.
  s.bands[1].enabled = false;
  auto comb = s;
  comb.bands[2] = setting(eq::Comb, eq::Unspecified, 500.).bands[0];
  eq::Engine waiting, reference;
  waiting.prepare(rate, 2, s);
  reference.prepare(rate, 2, s);
  bool equal = true;
  {
    Counter counter;
    for(int block = 0; block < 16; ++block)
    {
      waiting.update(comb);
      reference.update(s);
      waiting.process(in, 2, out, 2, frames);
      reference.process(in, 2, out_other, 2, frames);
      equal = equal && output == other;
    }
  }
  CHECK(allocations == 0);
  CHECK(equal);
  const int missing = waiting.missing_storage();
  REQUIRE(missing == 2);
  auto storage = eq::Engine::allocate(missing);
  {
    Counter counter;
    waiting.adopt(storage);
    for(int block = 0; block < 16; ++block)
    {
      waiting.update(comb);
      reference.update(s);
      waiting.process(in, 2, out, 2, frames);
      reference.process(in, 2, out_other, 2, frames);
    }
  }
  CHECK(allocations == 0);
  CHECK(waiting.missing_storage() == 0);
  CHECK_FALSE(output == other);
}

TEST_CASE("Parametriq reserves comb storage only for comb bands", "[parametriq]")
{
  eq::Engine warm;
  warm.prepare(rate, 1, {});
  long plain{}, comb{};
  {
    eq::Engine e;
    Counter counter;
    e.prepare(rate, 8, {});
    plain = allocated_bytes;
  }
  auto s = setting(eq::Comb, eq::Unspecified, 300.);
  {
    eq::Engine e;
    Counter counter;
    e.prepare(rate, 8, s);
    comb = allocated_bytes;
  }
  INFO("8 channels: " << plain << " bytes, with a comb band " << comb);
  CHECK(plain < 2'000'000);
  // Two copies of one line per channel: the comb's own and a transition's.
  constexpr long line = long(sizeof(float)) * (2048 * 64 + 12);
  CHECK(comb - plain == 2 * 8 * line);
}

TEST_CASE("Parametriq asks its worker for comb storage and to retire replaced states", "[parametriq]")
{
  using Fx = examples::ParametricEq;
  Fx fx;
  std::vector<Fx::job> jobs;
  fx.worker.request = [&](Fx::job j) { jobs.push_back(std::move(j)); };
  constexpr int frames = 64;
  std::array<std::array<double, frames>, 3> input{}, output{};
  std::array<double*, 3> in{}, out{};
  for(int c = 0; c < 3; ++c)
  {
    in[c] = input[c].data();
    out[c] = output[c].data();
  }
  fx.inputs.audio.samples = in.data();
  fx.outputs.audio.samples = out.data();
  fx.inputs.audio.channels = fx.outputs.audio.channels = 2;
  fx.prepare({.input_channels = 2, .output_channels = 2, .frames = frames, .rate = rate});
  CHECK(jobs.empty());
  fx.inputs.enabled1.value = true;
  fx.inputs.model1.value = eq::Comb;
  fx.inputs.frequency1.value = 1000.f;
  fx.inputs.resonance1.value = 0.9f;
  fx(frames);
  fx(frames);
  REQUIRE(jobs.size() == 1);
  REQUIRE(std::holds_alternative<int>(jobs[0]));
  CHECK(std::get<int>(jobs[0]) == 4);
  auto apply = decltype(fx.worker)::work(std::move(jobs[0]));
  REQUIRE(bool(apply));
  apply(fx);
  CHECK_FALSE(fx.storage_requested);
  CHECK(fx.engine.missing_storage() == 0);
  // The comb now rings: an impulse comes back a period later.
  double late{};
  for(int block = 0; block < 64; ++block)
  {
    input[0].fill(0.);
    if(block == 32)
      input[0][0] = 1.;
    fx(frames);
    if(block > 32)
      for(double v : output[0])
        late = std::max(late, std::abs(v));
  }
  CHECK(late > 0.05);

  fx.inputs.audio.channels = fx.outputs.audio.channels = 3;
  fx.prepare({.input_channels = 3, .output_channels = 3, .frames = frames, .rate = rate});
  REQUIRE(jobs.size() == 2);
  REQUIRE(std::holds_alternative<eq::Engine>(jobs[1]));
  CHECK(bool(std::get<eq::Engine>(jobs[1])));
  CHECK_FALSE(bool(decltype(fx.worker)::work(std::move(jobs[1]))));
}

TEST_CASE("Parametriq classic band transforms centre on their frequency", "[parametriq]")
{
  for(int model : {eq::Butterworth, eq::ChebyshevI})
    for(int response : {eq::Bell, eq::BandPass, eq::Notch})
      // Chebyshev bells of an even prototype order sit at the ripple's low edge at their centre.
      for(int order : {2, 6})
      {
        auto s = setting(model, response, 1000.);
        auto& p = s.bands[0];
        p.order = order;
        p.bandwidth = 0.7;
        p.gain = 12.;
        p.ripple = 1.;
        p = eq::sanitize(p, rate);
        INFO("model=" << model << " response=" << response << " order=" << order);
        std::vector<double> hz, db;
        for(double f = 700.; f <= 1300.; f += 0.05)
          hz.push_back(f);
        db.resize(hz.size());
        REQUIRE(eq::analytic_response(s, rate, hz, db));
        // A maximally flat top has no well-defined argmax: compare with the centre.
        const double centre = analytic(s, 1000.);
        if(response == eq::Notch)
        {
          CHECK(centre < -100.);
          CHECK(centre <= *std::min_element(db.begin(), db.end()) + 1.e-6);
        }
        else
        {
          CHECK(centre >= *std::max_element(db.begin(), db.end()) - 1.e-6);
          CHECK(centre == Catch::Approx(response == eq::Bell ? 12. : 0.).margin(1.e-6));
        }
        if(model == eq::Butterworth && response == eq::Bell && order == 2)
        {
          // The prototype is at half its gain at its edges, 0.7 octave apart.
          std::array<double, 2> edges{};
          for(int side = 0; side < 2; ++side)
          {
            double lo = side ? 1000. : 100., hi = side ? 5000. : 1000.;
            for(int i = 0; i < 80; ++i)
            {
              const double mid = 0.5 * (lo + hi);
              std::array<double, 1> f{mid}, v{};
              REQUIRE(eq::analytic_response(s, rate, f, v));
              ((v[0] > 6.) == (side == 0) ? hi : lo) = mid;
            }
            edges[side] = 0.5 * (lo + hi);
          }
          CHECK(std::log2(edges[1] / edges[0]) == Catch::Approx(0.7).margin(1.e-9));
          // Geometrically symmetric around the frequency once prewarped.
          const auto t = [](double f) { return std::tan(std::numbers::pi * f / rate); };
          CHECK(t(edges[0]) * t(edges[1]) == Catch::Approx(t(1000.) * t(1000.)).epsilon(1.e-9));
        }
      }
}

TEST_CASE("Parametriq classic band widths are octaves between their edges at every frequency and rate", "[parametriq]")
{
  // The edge level of each prototype: -3 dB for the Butterworth band-pass and
  // notch, half the gain for its bell, the ripple below the band for Chebyshev I.
  struct Case { int model, response; double gain, ripple, edge; };
  for(const auto c : std::array<Case, 5>{{
        {eq::Butterworth, eq::BandPass, 0., 0., -10. * std::log10(2.)},
        {eq::Butterworth, eq::Notch, 0., 0., -10. * std::log10(2.)},
        {eq::Butterworth, eq::Bell, 12., 0., 6.},
        {eq::ChebyshevI, eq::BandPass, 0., 1., -1.},
        {eq::ChebyshevI, eq::Bell, 12., 1., 11.}}})
    for(double fs : {48000., 192000.})
      for(int quality : {0, 2})
        for(double width : {1. / 3., 1.})
          for(double frequency : {100., 1000., 10000.})
          {
            eq::Settings s = setting(c.model, c.response, frequency);
            s.quality = quality;
            auto& p = s.bands[0];
            p.order = 2;
            p.bandwidth = width;
            p.gain = c.gain;
            p.ripple = c.ripple;
            p = eq::sanitize(eq::resolve(p, eq::Order), fs);
            REQUIRE(p.bandwidth == width);
            INFO("model=" << c.model << " response=" << c.response << " rate=" << fs
                 << " quality=" << quality << " width=" << width << " frequency=" << frequency);
            const auto level = [&](double f) {
              std::array<double, 1> hz{f}, db{};
              REQUIRE(eq::analytic_response(s, fs, hz, db));
              return db[0];
            };
            // The level crosses the edge level once on each side of the centre.
            const bool above = level(frequency) > c.edge;
            std::array<double, 2> edges{};
            for(int side = 0; side < 2; ++side)
            {
              double lo = side ? frequency : frequency / 8., hi = side ? std::min(frequency * 8., fs * 0.4999) : frequency;
              for(int i = 0; i < 80; ++i)
              {
                const double mid = std::sqrt(lo * hi);
                ((level(mid) > c.edge) == above ? (side ? lo : hi) : (side ? hi : lo)) = mid;
              }
              edges[side] = std::sqrt(lo * hi);
            }
            CHECK(std::log2(edges[1] / edges[0]) == Catch::Approx(width).margin(1.e-6));
            const auto t = [&](double f) { return std::tan(std::numbers::pi * f / (fs * (1 << quality))); };
            CHECK(t(edges[0]) * t(edges[1]) == Catch::Approx(t(frequency) * t(frequency)).epsilon(1.e-6));
          }
}

TEST_CASE("Parametriq classic band edges stay below Nyquist", "[parametriq]")
{
  for(double fs : {22050., 44100., 48000., 192000.})
    for(int quality : {0, 1, 2})
      for(double frequency : {20., 1000., 9000., 20000.})
      {
        eq::Settings s = setting(eq::ChebyshevI, eq::BandPass, frequency);
        s.quality = quality;
        auto& p = s.bands[0];
        p.order = 14;
        p.bandwidth = 8.;
        p = eq::sanitize(eq::resolve(p, eq::Order), fs);
        INFO("rate=" << fs << " quality=" << quality << " frequency=" << frequency);
        CHECK(p.bandwidth >= 0.0625);
        CHECK(p.bandwidth <= 8.);
        const auto edges = eq::band_edges(p.frequency, p.bandwidth, fs * (1 << quality));
        CHECK(edges.low > 0.);
        CHECK(edges.high <= 0.499 * fs * (1 << quality) * (1. + 1.e-9));
        CHECK(edges.high / edges.low == Catch::Approx(std::exp2(p.bandwidth)).epsilon(1.e-9));
        std::array<double, 64> input{}, output{};
        for(int i = 0; i < 64; ++i)
          input[i] = std::sin(0.3 * i);
        eq::Engine e;
        e.prepare(fs, 1, s);
        for(int k = 0; k < 100; ++k)
        {
          process(e, input.data(), output.data(), 64);
          for(double v : output)
            REQUIRE(std::isfinite(v));
        }
      }
}

namespace
{
struct NullPainter
{
  struct Color { unsigned char r, g, b, a; };
  void begin_path() { }
  void close_path() { }
  void stroke() { }
  void fill() { }
  void move_to(double, double) { }
  void line_to(double, double) { }
  void arc_to(double, double, double, double, double, double) { }
  void cubic_to(double, double, double, double, double, double) { }
  void quad_to(double, double, double, double) { }
  void translate(double, double) { }
  void scale(double, double) { }
  void rotate(double) { }
  void reset_transform() { }
  void set_stroke_color(Color) { }
  void set_stroke_width(double) { }
  void set_fill_color(Color) { }
  void set_font(const char*) { }
  void set_font_size(double) { }
  void draw_text(double, double, const char*) { }
  void draw_line(double, double, double, double) { }
  void draw_rect(double, double, double, double) { }
  void draw_rounded_rect(double, double, double, double, double) { }
  void draw_pixmap(double, double, const char*) { }
  void draw_ellipse(double, double, double, double) { }
  void draw_circle(double, double, double) { }
  void draw_bytes(double, double, double, double, const unsigned char*, int, int) { }
  void draw_bytes(double, double, double, double, const float*, int, int) { }
};
static_assert(avnd::painter<NullPainter>);
}

TEST_CASE("Parametriq painting never finishes an undoable edit", "[parametriq][ui]")
{
  GraphFixture f;
  int polls{};
  f.graph.request_poll = [&] { ++polls; };
  auto p = setting(eq::RBJ, eq::LowPass).bands[0];
  f.assign<0>(p);
  const double px = f.graph.x(p.frequency), py = f.graph.y(0.);
  REQUIRE(f.graph.wheel(px, py, 120.));
  REQUIRE(f.starts == 1);
  // The band's model changes elsewhere, e.g. from its combo box.
  p = f.graph.settings().bands[0];
  p.model = eq::Ivantsov;
  f.assign<0>(p);
  polls = 0;
  f.graph.paint(NullPainter{});
  CHECK(f.commits == 0);
  CHECK(polls > 0);
  f.graph.poll();
  CHECK(f.commits == 1);
}

TEST_CASE("Parametriq curve follows execution values and the host sample rate", "[parametriq][ui]")
{
  GraphFixture f;
  f.assign<0>(setting(eq::RBJ, eq::Bell).bands[0]);
  REQUIRE_FALSE(f.graph.poll());
  const auto peak_at = [&] {
    const auto peak = std::min_element(f.graph.ordinate.begin(), f.graph.ordinate.end());
    return f.graph.frequency_at(f.graph.left + f.graph.plot_width
        * std::distance(f.graph.ordinate.begin(), peak) / (eq::response_points - 1));
  };
  CHECK(peak_at() == Catch::Approx(1000.).epsilon(0.01));
  constexpr auto frequency = eq::Frequency;
  constexpr auto reference = eq::band_count * eq::field_count + 1;
  std::get<frequency>(f.graph.execution_values) = 3000.f;
  std::get<reference>(f.graph.execution_values) = -6.f;
  CHECK(f.graph.stale());
  REQUIRE_FALSE(f.graph.poll());
  CHECK(peak_at() == Catch::Approx(3000.).epsilon(0.01));
  CHECK(f.graph.cached.reference_db == -6.);
  // The handle stays where the user put it.
  CHECK(f.graph.settings().bands[0].frequency == Catch::Approx(1000.));
  std::get<frequency>(f.graph.execution_values).reset();
  std::get<reference>(f.graph.execution_values).reset();
  REQUIRE_FALSE(f.graph.poll());
  CHECK(peak_at() == Catch::Approx(1000.).epsilon(0.01));
  CHECK(f.graph.cached.reference_db == -18.);

  f.graph.sample_rate = 22050.;
  CHECK(f.graph.stale());
  REQUIRE_FALSE(f.graph.poll());
  CHECK(f.graph.cached_rate == 22050.);
  CHECK(f.graph.upper_frequency() == Catch::Approx(22050. * 0.49));
}

TEST_CASE("Parametriq linear bands stay bounded when every control jumps on every block", "[parametriq]")
{
  // Frequency, gain, Q and width jump across their ranges every 32 frames:
  // large steps restart the band rather than carry its states far away.
  for(int model : {eq::Butterworth, eq::ChebyshevI, eq::RBJ, eq::Ivantsov})
    for(int response = eq::LowPass; response <= eq::HighShelf; ++response)
      for(int order : {2, 14})
      {
        eq::Settings s;
        auto& p = s.bands[0];
        p.enabled = true;
        p.model = model;
        p.response = response;
        p = eq::resolve(p, eq::ResponseField);
        if(p.response != response)
          continue;
        p.order = order;
        p = eq::resolve(p, eq::Order);
        eq::Engine e;
        e.prepare(44100., 1, s);
        std::array<double, 32> input{}, output{};
        double peak{};
        for(int k = 0; k < 600; ++k)
        {
          p.frequency = 20. * std::pow(1000., (k % 400) / 400.);
          p.gain = -24. + 48. * ((k * 7) % 100) / 100.;
          p.q = 0.05 * std::pow(1000., ((k * 13) % 100) / 100.);
          p.bandwidth = 0.0625 * std::pow(128., ((k * 17) % 100) / 100.);
          for(int i = 0; i < 32; ++i)
            input[i] = std::sin(k * 32 + i);
          e.update(s);
          process(e, input.data(), output.data(), 32);
          for(double v : output)
            peak = std::max(peak, std::abs(v));
        }
        INFO("model=" << model << " response=" << response << " order=" << order);
        // Q 50 resonances and +24 dB gains: the static peaks are below 60.
        CHECK(peak < 200.);
      }
}

namespace
{
// Lowest output peak over 20 ms windows while the band alternates between its
// settings and `other` on every 64-frame block, relative to its steady peak,
// in dB. Restarting the band at each change would make it fall.
double alternation_drop(eq::Settings s, const eq::Parameters& other, double tone_hz)
{
  constexpr int frames = 64;
  eq::Engine e;
  e.prepare(rate, 1, s);
  std::array<double, frames> input{}, output{};
  long n{};
  double steady{}, lowest = std::numeric_limits<double>::infinity(), window{};
  const auto base = s.bands[0];
  const int settle = int(2 * rate / frames), per_window = int(0.02 * rate / frames);
  for(int b = 0; b < settle + int(rate / frames); ++b)
  {
    if(b >= settle)
      s.bands[0] = (b & 1) ? other : base;
    for(auto& x : input)
      x = 0.1 * std::sin(2. * std::numbers::pi * tone_hz * double(n++) / rate);
    e.update(s);
    process(e, input.data(), output.data(), frames);
    for(double v : output)
    {
      REQUIRE(std::isfinite(v));
      if(b < settle)
        steady = b > settle / 2 ? std::max(steady, std::abs(v)) : steady;
      else
        window = std::max(window, std::abs(v));
    }
    if(b >= settle && (b - settle + 1) % per_window == 0)
    {
      lowest = std::min(lowest, window);
      window = 0.;
    }
  }
  return 20. * std::log10(lowest / steady);
}
// Peak of the band's output over 300 ms of a unit sine after a step from a
// to b, relative to the larger of the two settings' steady peaks, in dB.
double step_overshoot(const eq::Parameters& a, const eq::Parameters& b, double tone_hz)
{
  constexpr int frames = 32;
  const auto run = [&](eq::Engine& e, eq::Settings& s, long& n, int blocks) {
    std::array<double, frames> input{}, output{};
    double peak{};
    for(int k = 0; k < blocks; ++k)
    {
      for(auto& x : input)
        x = std::sin(2. * std::numbers::pi * tone_hz * double(n++) / rate);
      e.update(s);
      process(e, input.data(), output.data(), frames);
      for(double v : output)
      {
        REQUIRE(std::isfinite(v));
        peak = std::max(peak, std::abs(v));
      }
    }
    return peak;
  };
  const int settle = int(rate / frames), measure = int(0.3 * rate / frames);
  const auto steady = [&](const eq::Parameters& p) {
    eq::Settings s;
    s.bands[0] = p;
    eq::Engine e;
    e.prepare(rate, 1, s);
    long n{};
    run(e, s, n, settle);
    return run(e, s, n, measure);
  };
  eq::Settings s;
  s.bands[0] = a;
  eq::Engine e;
  e.prepare(rate, 1, s);
  long n{};
  run(e, s, n, settle);
  s.bands[0] = b;
  return 20. * std::log10(run(e, s, n, measure) / std::max(steady(a), steady(b)));
}
}

TEST_CASE("Parametriq linear gain steps of any size keep the band sounding", "[parametriq]")
{
  for(int model : {eq::Butterworth, eq::ChebyshevI})
    for(int response : {eq::Bell, eq::LowShelf})
    {
      auto s = setting(model, response, 200.);
      auto& p = s.bands[0];
      p.order = 4;
      p.bandwidth = 0.07;
      p.gain = 12.;
      p.ripple = 1.;
      p = eq::sanitize(eq::resolve(p, eq::Order), rate);
      auto other = p;
      other.gain = 24.;
      INFO("model=" << model << " response=" << response);
      CHECK(alternation_drop(s, eq::sanitize(other, rate), 200.) > -0.5);
    }
}

TEST_CASE("Parametriq linear steps peak no higher than either setting", "[parametriq]")
{
  struct Case
  {
    int model, response;
    double frequency, ripple, gain, ripple_to, gain_to, tone;
  };
  for(const auto c : std::array<Case, 5>{{
        // Chebyshev poles move with the ripple's epsilon, not its dB: 0.02 to
        // 3 dB moves them by four octaves of epsilon.
        {eq::ChebyshevI, eq::LowPass, 200., 0.02, 0., 3., 0., 150.},
        {eq::ChebyshevI, eq::HighPass, 200., 3., 0., 0.02, 0., 250.},
        // Gains move the running filter, whatever the step.
        {eq::Butterworth, eq::Bell, 1000., 0., 0., 0., 24., 1000.},
        {eq::ChebyshevI, eq::Bell, 1000., 1., -12., 1., 24., 1000.},
        {eq::Butterworth, eq::LowShelf, 200., 0., -24., 0., 24., 100.}}})
  {
    eq::Parameters a = setting(c.model, c.response, c.frequency).bands[0];
    a.order = 4;
    a.bandwidth = 0.0625;
    a.ripple = c.ripple;
    a.gain = c.gain;
    a = eq::sanitize(eq::resolve(a, eq::Order), rate);
    auto b = a;
    b.ripple = c.ripple_to;
    b.gain = c.gain_to;
    INFO("model=" << c.model << " response=" << c.response);
    CHECK(step_overshoot(a, eq::sanitize(b, rate), c.tone) < 1.5);
  }
}

namespace
{
// Peak of the band's output over 100 ms of white noise after a step from a to
// b, relative to the larger of the two settings' steady peaks on the same
// noise, in dB: the worst of `sequences` noise sequences.
double noise_step_overshoot(const eq::Parameters& a, const eq::Parameters& b, unsigned sequences)
{
  constexpr int frames = 32;
  const auto run = [](eq::Engine& e, eq::Settings& s, unsigned& seed, double seconds) {
    std::array<double, frames> input{}, output{};
    double peak{};
    for(int k = 0; k < int(seconds * rate / frames); ++k)
    {
      for(auto& x : input)
      {
        seed = seed * 1664525u + 1013904223u;
        x = (seed >> 8) / double(1 << 24) * 2. - 1.;
      }
      e.update(s);
      process(e, input.data(), output.data(), frames);
      for(double v : output)
      {
        REQUIRE(std::isfinite(v));
        peak = std::max(peak, std::abs(v));
      }
    }
    return peak;
  };
  // Settles on `from` with one sequence, then plays another through `to`.
  const auto peak = [&](const eq::Parameters& from, const eq::Parameters& to, unsigned sequence) {
    eq::Settings s;
    s.bands[0] = from;
    eq::Engine e;
    e.prepare(rate, 1, s);
    unsigned seed = sequence;
    run(e, s, seed, 0.3);
    s.bands[0] = to;
    seed = sequence + 1000;
    return run(e, s, seed, 0.1);
  };
  double worst = -std::numeric_limits<double>::infinity();
  for(unsigned sequence = 1; sequence <= sequences; ++sequence)
    worst = std::max(worst, 20. * std::log10(peak(a, b, sequence)
                                 / std::max(peak(a, a, sequence), peak(b, b, sequence))));
  return worst;
}
}

TEST_CASE("Parametriq classic sections share the design's gain", "[parametriq]")
{
  // A step of a quarter octave of the edges' distance, 140.886 to 118.471 Hz,
  // of a 14-pole Chebyshev band-pass: with the
  // whole gain on its first section, the signal between sections would span
  // orders of magnitude and the states carried would ring well above both
  // settings.
  eq::Parameters a = setting(eq::ChebyshevI, eq::BandPass, 541.478).bands[0];
  a.order = 14;
  a.bandwidth = 0.3743261649655398;
  a.ripple = 1.;
  a = eq::sanitize(eq::resolve(a, eq::Order), rate);
  auto b = a;
  b.bandwidth = 0.3150269110874246;
  CHECK(noise_step_overshoot(a, eq::sanitize(b, rate), 16) < 3.);
}

namespace
{
eq::ResponseResult measure_response(const eq::Settings& s)
{
  eq::ResponseService service;
  const auto revision = service.request(s, rate);
  eq::ResponseResult result;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while(!service.take(result) && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  REQUIRE(result.revision == revision);
  return result;
}
std::array<double, eq::response_points> grid()
{
  std::array<double, eq::response_points> frequencies;
  for(int i = 0; i < eq::response_points; ++i)
    frequencies[i] = 20. * std::pow(1000., double(i) / (eq::response_points - 1));
  return frequencies;
}
eq::Parameters with_name(eq::Parameters p, eq::Field field, std::string_view name)
{
  for(const auto& c : eq::choices(p, field))
    if(c.name == name)
    {
      switch(field)
      {
        case eq::ResponseField: p.response = c.value; break;
        case eq::Character: p.character = c.value; break;
        default: break;
      }
    }
  return eq::resolve(p, field);
}
// The reported settings: a self-oscillating OB-Xd low-pass and a low shelf.
eq::Settings oscillating()
{
  eq::Settings s;
  auto& p = s.bands[0];
  p.enabled = true;
  p.model = eq::OBXd2;
  p = with_name(with_name(p, eq::ResponseField, "Low-pass"), eq::Character, "Standard");
  p.frequency = 1030.;
  p.resonance = 1.;
  auto& shelf = s.bands[1];
  shelf.enabled = true;
  shelf.model = eq::RBJ;
  shelf.response = eq::LowShelf;
  shelf.frequency = 40.;
  shelf.gain = 2.;
  shelf = eq::resolve(shelf);
  s.quality = 0;
  s.reference_db = -18.;
  return s;
}
}

TEST_CASE("Parametriq measures a self-oscillating band beside its oscillation", "[parametriq]")
{
  const auto s = oscillating();
  REQUIRE(s.bands[0].character == eq::resolve(s.bands[0]).character);
  const auto result = measure_response(s);
  REQUIRE(result.status == eq::ResponseStatus::Reference);
  int estimated = 0;
  for(int i = 0; i < eq::response_points; ++i)
  {
    REQUIRE(std::isfinite(result.db[i]));
    estimated += result.estimated[i];
  }
  // Away from its cutoff, the probe passes as it does through a long run of
  // the oscillating filter.
  eq::Engine e;
  e.prepare(rate, 1, s);
  const double amplitude = std::pow(10., s.reference_db / 20.);
  const auto frequencies = grid();
  for(int i : {120, 200, 340, 420})
  {
    e.reset();
    CHECK(result.db[i] == Catch::Approx(tone(e, frequencies[i], amplitude)).margin(0.3));
  }
  CHECK(estimated <= 16);
  // It resonates up to its peak.
  const auto peak = std::max_element(result.db.begin() + 240, result.db.begin() + 340);
  CHECK(*peak > 12.);

  // The shelf's shape is all there, whatever the oscillating band does: the
  // chain with it is the chain without it, times its exact response.
  auto without = s;
  without.bands[1].enabled = false;
  const auto alone = measure_response(without);
  REQUIRE(alone.status == eq::ResponseStatus::Reference);
  eq::Settings shelf;
  shelf.bands[0] = s.bands[1];
  std::array<double, eq::response_points> exact{};
  REQUIRE(eq::analytic_response(shelf, rate, frequencies, exact));
  for(int i = 0; i < eq::response_points; ++i)
    CHECK(result.db[i] - alone.db[i] == Catch::Approx(exact[i]).margin(0.3));
}

TEST_CASE("Parametriq draws every nonlinear model at every resonance", "[parametriq]")
{
  int curves = 0;
  for(int model = eq::VemberClassic; model < eq::model_count; ++model)
  {
    if(model == eq::FastSVF || model == eq::SampleHold)
      continue;
    eq::Parameters base;
    base.enabled = true;
    base.model = model;
    base = eq::resolve(base);
    for(const auto& response : eq::choices(base, eq::ResponseField))
    {
      auto p = base;
      p.response = response.value;
      p = eq::resolve(p, eq::ResponseField);
      for(double resonance : {0., 0.5, 0.9, 1.})
      {
        CAPTURE(eq::model_names()[model], response.name, resonance);
        eq::Settings s;
        s.bands[0] = p;
        s.bands[0].resonance = resonance;
        const auto result = measure_response(s);
        REQUIRE(result.status == eq::ResponseStatus::Reference);
        int estimated = 0, visible = 0;
        for(int i = 0; i < eq::response_points; ++i)
        {
          REQUIRE(std::isfinite(result.db[i]));
          estimated += result.estimated[i];
          visible += result.estimated[i] && result.db[i] > -36.;
        }
        // Dashed spans stay narrow on the plot. The tri-pole is chaotic at high
        // resonance: its probe's gain scatters by a dB from window to window
        // over octaves around its frequency.
        CHECK(visible <= (model == eq::Tripole ? 128 : 16));
        CHECK(estimated <= (model == eq::Tripole ? 256 : 64));
        // Levels place the handle on the curve.
        CHECK(result.levels[0].valid);
        for(double level : result.levels[0].db)
          CHECK(std::isfinite(level));
        ++curves;
      }
    }
  }
  CHECK(curves > 100);
}

TEST_CASE("Parametriq handles of bands without gain sit on their own level", "[parametriq][ui]")
{
  GraphFixture f;
  // A resonant RBJ cut peaks at its Q at its frequency.
  auto p = setting(eq::RBJ, eq::LowPass).bands[0];
  for(double q : {0.3, 0.7071067811865476, 2., 10.})
  {
    p.q = q;
    f.assign<0>(p);
    CHECK(f.graph.handle_level(0, eq::sanitize(p, rate)) == Catch::Approx(20. * std::log10(q)).margin(0.05));
  }
  // A Butterworth cut, with nothing to shape, at its -3 dB.
  const auto butterworth = setting(eq::Butterworth, eq::HighPass).bands[0];
  f.assign<0>(butterworth);
  CHECK(f.graph.handle_level(0, eq::sanitize(butterworth, rate)) == Catch::Approx(-3.01).margin(0.05));

  // A nonlinear band, once measured: rising with its resonance, near its curve.
  auto obxd = with_name(setting(eq::OBXd2, eq::LowPass).bands[0], eq::Character, "Standard");
  double previous = -100.;
  for(double resonance : {0., 0.5, 0.9, 1.})
  {
    obxd.resonance = resonance;
    f.assign<0>(obxd);
    f.settle();
    REQUIRE(f.graph.status == eq::ResponseStatus::Reference);
    REQUIRE(f.graph.measured_levels[0].valid);
    const double level = f.graph.handle_level(0, eq::sanitize(obxd, rate));
    CHECK(level > previous + 1.);
    previous = level;
    if(resonance == 0.5)
    {
      const int i = int(std::lround(std::log(obxd.frequency / 20.) / std::log(1000.) * (eq::response_points - 1)));
      CHECK(f.handle_y(0) == Catch::Approx(f.graph.ordinate[i]).margin(3.));
    }
  }
}

TEST_CASE("Parametriq vertical drags move the handle of a band without gain to the mouse", "[parametriq][ui]")
{
  struct Case { int model, response; };
  for(const auto [model, response] : std::array<Case, 9>{{
          {eq::RBJ, eq::LowPass}, {eq::RBJ, eq::HighPass}, {eq::RBJ, eq::BandPass},
          {eq::RBJ, eq::Notch}, {eq::RBJ, eq::AllPass}, {eq::Butterworth, eq::BandPass},
          {eq::Ivantsov, eq::LowPass}, {eq::OBXd2, eq::LowPass}, {eq::K35, eq::HighPass}}})
  {
    CAPTURE(model, response);
    GraphFixture f;
    auto p = setting(model, response).bands[0];
    if(model == eq::OBXd2 || model == eq::K35)
      p.resonance = 0.3;
    f.assign<0>(p);
    f.settle();
    const auto field = eq::shape_field(eq::sanitize(p, rate));
    REQUIRE(field != eq::field_count);
    const double px = f.graph.x(p.frequency);
    const double start = f.handle_y(0);
    REQUIRE(f.graph.mouse_press(Mouse{px, start}));
    // The handle follows the mouse, up and down, within the parameter's range.
    for(double dy : {-20., -40., 5., -10.})
    {
      f.graph.mouse_move(Mouse{px, start + dy});
      CHECK(f.handle_y(0) == Catch::Approx(start + dy).margin(0.5));
    }
    // Past its range, it stops at the limit.
    f.graph.mouse_move(Mouse{px, -400.});
    const auto top = eq::sanitize(f.graph.settings().bands[0], rate);
    CHECK(eq::shape_position(top, field) == Catch::Approx(1.).margin(1.e-9));
    f.graph.mouse_move(Mouse{px, 800.});
    const auto bottom = eq::sanitize(f.graph.settings().bands[0], rate);
    CHECK(eq::shape_position(bottom, field) < 0.05);
    f.graph.mouse_move(Mouse{px, start - 20.});
    f.graph.mouse_release(Mouse{px, start - 20.});
    CHECK(f.commits == 1);
    // Released, the handle stays where it was dropped, under the mouse, and
    // is hit there.
    CHECK(f.handle_y(0) == Catch::Approx(start - 20.).margin(model == eq::OBXd2 || model == eq::K35 ? 4. : 0.5));
    CHECK(f.graph.hit(px, start - 20.) == 0);
    const auto after = eq::sanitize(f.graph.settings().bands[0], rate);
    CHECK(eq::shape_value(after, field) != eq::shape_value(eq::sanitize(p, rate), field));
    CHECK(after.frequency == Catch::Approx(eq::sanitize(p, rate).frequency));
  }
}

TEST_CASE("Parametriq alt drags edit the shape alone", "[parametriq][ui]")
{
  constexpr int alt = Mouse::alt;
  // A bell keeps its gain and frequency; its handle stays at its gain.
  {
    GraphFixture f;
    const auto p = setting(eq::RBJ, eq::Bell).bands[0];
    f.assign<0>(p);
    const double px = f.graph.x(p.frequency), py = f.handle_y(0);
    REQUIRE(f.graph.mouse_press(Mouse{px, py, 1, alt}));
    f.graph.mouse_move(Mouse{px + 40., py - 30., 1, alt});
    f.graph.mouse_release(Mouse{px + 40., py - 30., 1, alt});
    const auto after = f.graph.settings().bands[0];
    CHECK(after.q > p.q);
    CHECK(after.gain == p.gain);
    CHECK(after.frequency == p.frequency);
    CHECK(f.handle_y(0) == Catch::Approx(py));
  }
  // A cut keeps its frequency; its handle follows the mouse vertically.
  {
    GraphFixture f;
    const auto p = setting(eq::RBJ, eq::LowPass).bands[0];
    f.assign<0>(p);
    const double px = f.graph.x(p.frequency), py = f.handle_y(0);
    REQUIRE(f.graph.mouse_press(Mouse{px, py, 1, alt}));
    f.graph.mouse_move(Mouse{px + 40., py - 30., 1, alt});
    f.graph.mouse_release(Mouse{px + 40., py - 30., 1, alt});
    const auto after = f.graph.settings().bands[0];
    CHECK(after.q > p.q);
    CHECK(after.frequency == p.frequency);
    CHECK(f.handle_y(0) == Catch::Approx(py - 30.).margin(0.5));
  }
}
