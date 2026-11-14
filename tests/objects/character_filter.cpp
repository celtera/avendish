#include <catch2/catch_all.hpp>
#include <examples/Advanced/Audio/CharacterFilter/Dsp.hpp>
#include <examples/Advanced/Audio/DynamicWaveshaper/Halfband.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <numbers>
#include <vector>

namespace
{
// While enabled, counts allocations and fills new memory with a byte pattern,
// so that reading memory nothing wrote changes the output.
std::atomic<bool> tracking{false};
std::atomic<long> allocations{0};
std::atomic<int> fill{0};
void* allocate(std::size_t n)
{
  void* p = std::malloc(n ? n : 1);
  if(!p)
    throw std::bad_alloc{};
  if(tracking.load(std::memory_order_relaxed))
  {
    allocations.fetch_add(1, std::memory_order_relaxed);
    std::memset(p, fill.load(std::memory_order_relaxed), n);
  }
  return p;
}
struct Tracking
{
  explicit Tracking(int pattern = 0)
  {
    allocations = 0;
    fill = pattern;
    tracking = true;
  }
  ~Tracking() { tracking = false; }
  long count() const { return allocations.load(); }
};
}
void* operator new(std::size_t n) { return allocate(n); }
void* operator new[](std::size_t n) { return allocate(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
  try { return allocate(n); } catch(...) { return nullptr; }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
  try { return allocate(n); } catch(...) { return nullptr; }
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace cf = ao::character_filter;
namespace
{
std::vector<double> sine(int frames, double rate, double hz, double level = 0.1)
{
  std::vector<double> result(frames);
  for(int i = 0; i < frames; ++i)
    result[i] = level * std::sin(2. * std::numbers::pi * hz * i / rate);
  return result;
}
void render(cf::Engine& e, const std::vector<double>& input, std::vector<double>& output)
{
  const double* in = input.data();
  double* out = output.data();
  e.process(&in, 1, &out, 1, int(input.size()));
}
// The input through the 2x oversampling alone, as the engine runs it.
std::vector<double> oversampled(const std::vector<double>& input)
{
  ao::dynamic_waveshaper::halfband::Interpolator2x up;
  ao::dynamic_waveshaper::halfband::Decimator2x down;
  std::vector<double> result(input.size());
  for(std::size_t i = 0; i < input.size(); ++i)
  {
    float a, b;
    up.process(float(input[i]), a, b);
    result[i] = down.process(a, b);
  }
  return result;
}
double energy(const std::vector<double>& x, int start = 0)
{
  double sum = 0.;
  for(int i = start; i < int(x.size()); ++i)
    sum += x[i] * x[i];
  return sum;
}
}

TEST_CASE("Character filter configurations remain bounded and isolate SIMD lanes", "[character-filter]")
{
  constexpr int frames = 2048;
  std::vector<double> silence(frames), left(frames), right(frames);
  for(double rate : {8000., 44100., 48000., 96000., 192000., 384000., 768000.})
  {
    const auto input = sine(frames, rate, 173.);
    for(int model = 0; model < int(cf::models.size()); ++model)
    {
      for(int variant = 0; variant < cf::models[model].count; ++variant)
      {
        INFO(rate << " / " << cf::models[model].name << " / "
             << cf::configurations[cf::models[model].begin + variant].name);
        cf::Engine e;
        cf::Parameters p;
        p.model = model;
        p.variant = variant;
        p.resonance = 0.7f;
        e.prepare(rate, 2, p);
        const double* in[]{input.data(), silence.data()};
        double* out[]{left.data(), right.data()};
        e.process(in, 2, out, 2, frames);
        for(int i = 0; i < frames; ++i)
        {
          REQUIRE(std::isfinite(left[i]));
          REQUIRE(std::abs(left[i]) < 100.);
          REQUIRE(std::abs(right[i]) < 1.e-6);
        }
      }
    }
  }
}

TEST_CASE("Character filter cutoff controls the measured lowpass response", "[character-filter]")
{
  constexpr int frames = 16384;
  const auto input = sine(frames, 48000., 2000.);
  std::vector<double> low(frames), high(frames);
  cf::Parameters p;
  p.model = 10; // Cytomic SVF, lowpass
  p.cutoff = 200.f;
  cf::Engine e;
  e.prepare(48000., 1, p);
  render(e, input, low);
  p.cutoff = 6000.f;
  e.setParameters(p);
  render(e, input, high);
  REQUIRE(energy(high, frames / 2) > 100. * energy(low, frames / 2));
  REQUIRE(energy(high, frames / 2) < 2. * energy(input, frames / 2));
}

TEST_CASE("Character filter automation is independent of host block partition", "[character-filter]")
{
  constexpr int frames = 6000;
  const auto input = sine(frames, 48000., 317.);
  auto run = [&](const std::vector<int>& blocks) {
    cf::Engine e;
    cf::Parameters p;
    e.prepare(48000., 1, p);
    std::vector<double> output(frames);
    constexpr std::array events{29, 123, 990, 2000, 3500, frames};
    int pos = 0, event = 0, block = 0;
    while(pos < frames)
    {
      if(pos == events[event])
      {
        switch(event)
        {
          case 0: p.model = 12; p.cutoff = 700.f; break;
          case 1: p.model = 7; p.variant = 2; p.drive_db = 12.f; break;
          case 2: p.model = 11; p.variant = 4; p.resonance = 0.8f; break;
          case 3: p.model = 14; p.variant = 1; p.cutoff = 4000.f; break;
          case 4: p.enabled = false; break;
        }
        ++event;
      }
      const int n = std::min(events[event] - pos, blocks[block++ % blocks.size()]);
      e.setParameters(p);
      const double* in = input.data() + pos;
      double* out = output.data() + pos;
      e.process(&in, 1, &out, 1, n);
      pos += n;
    }
    return output;
  };
  const auto reference = run({64});
  const auto split = run({1, 7, 127, 3, 256, 17});
  for(int i = 0; i < frames; ++i)
    REQUIRE(split[i] == reference[i]);
}

TEST_CASE("Character filter model changes start continuously and bypass keeps the oversampling delay", "[character-filter]")
{
  auto input = sine(4096, 48000., 241.);
  std::vector<double> a(input.size()), b(input.size());
  cf::Engine changed, unchanged;
  cf::Parameters p;
  changed.prepare(48000., 1, p);
  unchanged.prepare(48000., 1, p);
  render(changed, input, a);
  render(unchanged, input, b);
  p.model = 12;
  changed.setParameters(p);
  render(changed, input, a);
  render(unchanged, input, b);
  // First sample still belongs wholly to the old bank, not reset state.
  REQUIRE(a[0] == b[0]);
  // The 10 ms fade adds at most a fraction of the bank difference per sample.
  double steady = 0., fading = 0.;
  for(int i = 1; i < 1024; ++i)
  {
    steady = std::max(steady, std::abs(b[i] - b[i - 1]));
    fading = std::max(fading, std::abs(a[i] - a[i - 1]));
  }
  REQUIRE(fading < 1.5 * steady);
  p.enabled = false;
  p.drive_db = 36.f;
  p.output_db = -60.f;
  changed.setParameters(p);
  render(changed, input, a);
  std::vector<double> stream;
  for(int k = 0; k < 3; ++k)
    stream.insert(stream.end(), input.begin(), input.end());
  const auto through = oversampled(stream);
  for(int i = 4000; i < 4096; ++i)
    REQUIRE(std::abs(a[i] - through[2 * 4096 + i]) < 1.e-7);
}

TEST_CASE("Character filter bypass is exactly the input through the oversampling", "[character-filter]")
{
  auto input = sine(4096, 48000., 241.);
  input[1000] = 1.e300;
  input[1001] = std::numeric_limits<double>::quiet_NaN();
  cf::Parameters p;
  p.enabled = false;
  p.drive_db = 36.f;
  cf::Engine e;
  e.prepare(48000., 1, p);
  std::vector<double> output(input.size());
  render(e, input, output);
  auto bounded = input;
  bounded[1000] = 64.;
  bounded[1001] = 0.;
  const auto through = oversampled(bounded);
  for(int i = 0; i < 4096; ++i)
    REQUIRE(output[i] == through[i]);
  // The oversampling delays an impulse by four to five samples.
  std::vector<double> impulse(64), response(64);
  impulse[0] = 1.;
  cf::Engine fresh;
  fresh.prepare(48000., 1, p);
  render(fresh, impulse, response);
  const auto peak = std::max_element(response.begin(), response.end(),
      [](double x, double y) { return std::abs(x) < std::abs(y); });
  REQUIRE(peak - response.begin() >= 4);
  REQUIRE(peak - response.begin() <= 5);
}

TEST_CASE("Character filter enable fade does not comb filter", "[character-filter]")
{
  // A 20 Hz highpass is transparent at 6 kHz, where a dry signal that skipped
  // the oversampling's delay would be in opposite phase with the filtered one.
  constexpr double rate = 48000., hz = 6000.;
  constexpr int frames = 24000, toggle = 12000, period = 8;
  const auto input = sine(frames, rate, hz, 0.25);
  std::vector<double> output(frames);
  cf::Parameters p;
  p.model = 0;
  p.variant = 7;
  REQUIRE(cf::configurations[7].name == "Highpass 12 dB Clean");
  p.cutoff = 20.f;
  p.resonance = 0.f;
  cf::Engine e;
  e.prepare(rate, 1, p);
  const double* in = input.data();
  double* out = output.data();
  e.process(&in, 1, &out, 1, toggle);
  p.enabled = false;
  e.setParameters(p);
  in += toggle;
  out += toggle;
  e.process(&in, 1, &out, 1, frames - toggle);
  auto peak = [&](int start) {
    double m = 0.;
    for(int i = start; i < start + period + 1; ++i)
      m = std::max(m, std::abs(output[i]));
    return m;
  };
  const double steady = std::min(peak(toggle - 4 * period), peak(frames - 4 * period));
  REQUIRE(steady > 0.24);
  for(int i = toggle - period; i < toggle + 2400; ++i)
    REQUIRE(peak(i) > 0.99 * steady);
}

TEST_CASE("Character filter reprepare resets comb tails and handles channel growth", "[character-filter]")
{
  constexpr int frames = 4096;
  const auto input = sine(frames, 48000., 400.);
  std::vector<double> zero(frames), output(frames), silent(frames);
  cf::Engine e;
  cf::Parameters p;
  p.model = 12;
  p.variant = 1;
  p.resonance = 0.9f;
  for(int channels : {1, 2, 5, 2})
  {
    e.prepare(48000., channels, p);
    std::array<const double*, 5> in{};
    std::array<double*, 5> out{};
    for(int c = 0; c < channels; ++c)
    {
      in[c] = zero.data();
      out[c] = silent.data();
    }
    e.process(in.data(), channels, out.data(), channels, frames);
    REQUIRE(energy(silent) == 0.);
    in[channels - 1] = input.data();
    out[channels - 1] = output.data();
    e.process(in.data(), channels, out.data(), channels, frames);
    REQUIRE(energy(output, frames / 2) > 0.01);
    if(channels > 1)
      REQUIRE(energy(silent) == 0.);
  }
}

TEST_CASE("Character filter catalog is accepted by the pinned SST", "[character-filter]")
{
  for(int i = 0; i < int(cf::configurations.size()); ++i)
  {
    INFO(i << " " << cf::configurations[i].name);
    REQUIRE(cf::configurationAvailable(i));
  }
  REQUIRE(!cf::configurationAvailable(-1));
  REQUIRE(!cf::configurationAvailable(int(cf::configurations.size())));
}

TEST_CASE("Character filter models stay valid at every rate, cutoff, resonance and drive",
          "[character-filter]")
{
  constexpr int frames = 4096;
  std::vector<double> output(frames);
  for(double rate : {8000., 44100., 96000., 192000., 384000., 768000.})
  {
    std::vector<double> input(frames);
    for(int i = 0; i < frames; ++i)
      input[i] = std::sin(2. * std::numbers::pi * 110. * i / rate) + (i % 1000 == 0 ? 1. : 0.);
    for(int model = 0; model < int(cf::models.size()); ++model)
      for(int variant = 0; variant < cf::models[model].count; ++variant)
        for(float cutoff : {20.f, 20000.f})
          for(float resonance : {0.f, 1.f})
            for(float drive : {0.f, 36.f})
            {
              cf::Parameters p;
              p.model = model;
              p.variant = variant;
              p.cutoff = cutoff;
              p.resonance = resonance;
              p.drive_db = drive;
              p.extra = 1.f;
              cf::Engine e;
              e.prepare(rate, 1, p);
              render(e, input, output);
              bool valid = true;
              for(double y : output)
                valid = valid && std::isfinite(y) && std::abs(y) < 1.e4;
              INFO(rate << " / " << cf::models[model].name << " / "
                   << cf::configurations[cf::models[model].begin + variant].name << " / "
                   << cutoff << " Hz / " << resonance << " / " << drive << " dB");
              REQUIRE(valid);
            }
  }
}

TEST_CASE("Character filter lattice configurations keep filtering at the lowest cutoff",
          "[character-filter]")
{
  // Vember Classic "Lowpass 12 dB Clean" and "Highpass 12 dB Clean".
  constexpr int lowpass = 1, highpass = 7;
  for(double rate : {48000., 384000., 768000.})
  {
    INFO(rate);
    const float lowest = cf::minimumCutoff(cf::models[0].begin + lowpass, rate);
    REQUIRE(lowest >= 20.f);
    REQUIRE(lowest < 150.f);
    REQUIRE(cf::minimumCutoff(cf::models[0].begin + highpass, rate) == lowest);
    // Models whose coefficients do not degenerate keep the 20 Hz minimum.
    REQUIRE(cf::minimumCutoff(cf::models[0].begin, rate) == 20.f);
    REQUIRE(cf::minimumCutoff(cf::models[10].begin, rate) == 20.f);

    const int frames = int(2. * rate);
    std::vector<double> silence(frames), output(frames);
    const auto low = sine(frames, rate, lowest * 0.25);
    const auto high = sine(frames, rate, lowest * 40.);
    auto run = [&](int variant, const std::vector<double>& input) {
      cf::Parameters p;
      p.variant = variant;
      p.cutoff = 20.f;
      p.resonance = 0.f;
      cf::Engine e;
      e.prepare(rate, 1, p);
      render(e, input, output);
      for(double y : output)
        REQUIRE(std::isfinite(y));
      return energy(output, frames / 2);
    };
    // Resonance 0 is overdamped: about -4 dB two octaves below the cutoff.
    REQUIRE(run(lowpass, silence) == 0.);
    REQUIRE(run(lowpass, low) > 0.25 * energy(low, frames / 2));
    REQUIRE(run(lowpass, high) < 0.01 * energy(high, frames / 2));
    REQUIRE(run(highpass, low) < 0.05 * energy(low, frames / 2));
    REQUIRE(run(highpass, high) > 0.5 * energy(high, frames / 2));
  }
}

TEST_CASE("Character filter recovers from huge and non-finite input samples", "[character-filter]")
{
  constexpr int frames = 24000;
  auto input = sine(frames, 48000., 220.);
  input[100] = 1.e38;
  input[200] = std::numeric_limits<double>::quiet_NaN();
  input[300] = std::numeric_limits<double>::infinity();
  input[400] = 1.e300;
  input[500] = -1.e300;
  input[600] = -std::numeric_limits<double>::infinity();
  std::vector<double> output(frames);
  for(int model = 0; model < int(cf::models.size()); ++model)
    for(int variant = 0; variant < cf::models[model].count; ++variant)
    {
      INFO(cf::models[model].name << " / "
           << cf::configurations[cf::models[model].begin + variant].name);
      cf::Parameters p;
      p.model = model;
      p.variant = variant;
      p.drive_db = 36.f;
      cf::Engine e;
      e.prepare(48000., 1, p);
      render(e, input, output);
      bool finite = true;
      for(double y : output)
        finite = finite && std::isfinite(y);
      REQUIRE(finite);
      REQUIRE(energy(output, frames - 4800) > 0.);
    }
}

TEST_CASE("Character filter processing and model changes do not allocate", "[character-filter]")
{
  constexpr int channels = 8, frames = 64;
  std::array<std::vector<double>, channels> in, out;
  std::array<const double*, channels> ip{};
  std::array<double*, channels> op{};
  for(int c = 0; c < channels; ++c)
  {
    in[c] = sine(frames, 48000., 300. + 50. * c);
    out[c].resize(frames);
    ip[c] = in[c].data();
    op[c] = out[c].data();
  }
  cf::Engine e;
  cf::Parameters p;
  e.prepare(48000., channels, p);
  const Tracking tracking;
  auto block = [&] { e.process(ip.data(), channels, op.data(), channels, frames); };
  for(int model = 0; model < int(cf::models.size()); ++model)
    for(int variant = 0; variant < cf::models[model].count; ++variant)
    {
      p.model = model;
      p.variant = variant;
      p.cutoff = variant % 2 ? 20.f : 20000.f;
      p.resonance = variant % 3 ? 1.f : 0.f;
      p.drive_db = variant % 2 ? 36.f : -24.f;
      p.extra = variant % 2 ? 1.f : 0.f;
      p.enabled = variant % 5 != 0;
      e.setParameters(p);
      for(int k = 0; k < 12; ++k)
        block();
    }
  in[0][7] = 1.e38;
  in[1][9] = std::numeric_limits<double>::quiet_NaN();
  block();
  const long count = tracking.count();
  REQUIRE(count == 0);
}

TEST_CASE("Character filter comb never reads memory it did not write", "[character-filter]")
{
  // Delay storage is left uninitialized except for what select() clears:
  // allocating it zero-filled or NaN-filled must not change any output.
  constexpr int channels = 5;
  for(double rate : {48000., 192000., 768000.})
  {
    INFO(rate);
    const int frames = int(rate * 1.5);
    const auto input = sine(frames, rate, 97.);
    auto run = [&](int pattern) {
      cf::Engine e;
      cf::Parameters p;
      p.model = 12;
      p.resonance = 0.9f;
      p.cutoff = 20.f;
      {
        const Tracking tracking(pattern);
        e.prepare(rate, channels, p);
      }
      std::vector<double> result;
      std::array<std::vector<double>, channels> out;
      std::array<const double*, channels> ip{};
      std::array<double*, channels> op{};
      constexpr int block = 512;
      for(int c = 0; c < channels; ++c)
        out[c].resize(block);
      for(int pos = 0; pos + block <= frames; pos += block)
      {
        // Sweep the delay over its whole range, and enter the comb afresh
        // on the other bank half way through.
        const double t = double(pos) / frames;
        p.cutoff = float(20. * std::pow(1000., std::abs(1. - 4. * std::fmod(t, 0.5))));
        p.model = (t > 0.5 && t < 0.52) ? 0 : 12;
        p.variant = t > 0.52 ? 6 : 0;
        e.setParameters(p);
        for(int c = 0; c < channels; ++c)
        {
          ip[c] = input.data() + pos;
          op[c] = out[c].data();
        }
        e.process(ip.data(), channels, op.data(), channels, block);
        for(int c = 0; c < channels; ++c)
          result.insert(result.end(), out[c].begin(), out[c].end());
      }
      return result;
    };
    const auto zeroed = run(0);
    const auto poisoned = run(0xFF);
    bool identical = zeroed.size() == poisoned.size();
    for(std::size_t i = 0; identical && i < zeroed.size(); ++i)
      identical = zeroed[i] == poisoned[i];
    REQUIRE(identical);
    REQUIRE(energy(zeroed) > 1.);
  }
}

TEST_CASE("Character filter banks follow their own model's extra control through a fade",
          "[character-filter]")
{
  // The extra sent with a model that has none must not reach the next model.
  const auto input = sine(4096, 48000., 300., 0.3);
  auto run = [&](float previous) {
    cf::Engine e;
    cf::Parameters p;
    p.extra = previous;
    e.prepare(48000., 1, p);
    std::vector<double> a(input.size()), b(input.size());
    render(e, input, a);
    p.model = 12;
    p.extra = 0.3f;
    e.setParameters(p);
    render(e, input, b);
    a.insert(a.end(), b.begin(), b.end());
    return a;
  };
  const auto low = run(0.f), high = run(1.f);
  for(std::size_t i = 0; i < low.size(); ++i)
    REQUIRE(low[i] == high[i]);
}
