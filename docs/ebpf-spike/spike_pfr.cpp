// Phase-0 spike (minimal): boost::pfr structural reflection + q15 fixed-point,
// lowered to BPF bytecode, with NO BPF scaffolding -- isolates the pure
// "does the reflection machinery lower to BPF?" question.
//
// Build: clang++ -target bpf -O2 -std=c++23 -fno-exceptions -fno-rtti \
//          <host c++ include dirs as -isystem> -c spike_pfr.cpp -o spike_pfr.o
// Inspect: llvm-objdump -d spike_pfr.o
#include <boost/pfr/core.hpp>
#include <cstdint>
#include <type_traits>

namespace pfr = boost::pfr;

struct q15 {
  std::int16_t v{};
  friend q15 operator*(q15 a, q15 b) noexcept {
    return { std::int16_t((std::int32_t(a.v) * b.v) >> 15) };
  }
};
struct audio_sample { q15 sample; };
struct knob         { q15 value; };
template <typename> constexpr bool is_audio = false;
template <> constexpr bool is_audio<audio_sample> = true;

struct Gain {
  struct { audio_sample audio; knob gain; } inputs;
  struct { audio_sample audio;            } outputs;
  void operator()() noexcept {
    outputs.audio.sample = inputs.audio.sample * inputs.gain.value;
  }
};

template <typename Fx>
static inline std::int16_t process_one(Fx& fx, std::int16_t in_raw) noexcept {
  pfr::for_each_field(fx.inputs,  [&](auto& f){ if constexpr (is_audio<std::decay_t<decltype(f)>>) f.sample.v = in_raw; });
  fx();
  std::int16_t out = 0;
  pfr::for_each_field(fx.outputs, [&](auto& f){ if constexpr (is_audio<std::decay_t<decltype(f)>>) out = f.sample.v; });
  return out;
}

struct ctx { std::int16_t* in; std::int16_t* out; std::int32_t frames; std::int16_t gain; };

extern "C" int bpf_audio(struct ctx* c) {
  Gain fx{};
  fx.inputs.gain.value.v = c->gain;
  #pragma unroll
  for (int i = 0; i < 256; ++i) {        // compile-time bound for the verifier
    if (i >= c->frames) break;
    c->out[i] = process_one(fx, c->in[i]);
  }
  return 0;
}
