// Phase-0 spike: a loadable XDP program where avendish-style boost::pfr
// reflection + q15 fixed-point coexist with real BPF scaffolding
// (SEC, BTF maps, helpers, packet bounds checks).
//
// Build:  ./build.sh        (produces xdp_gain.bpf.o + xdp_gain.skel.h)
// Load:   sudo bpftool prog loadall xdp_gain.bpf.o /sys/fs/bpf/xdp_gain
#include <linux/bpf.h>
#include "bpf_cxx.hpp"
#include <boost/pfr/core.hpp>   // STRUCTURAL reflection only -- NOT <boost/pfr.hpp>
#include <cstdint>
#include <type_traits>

namespace pfr = boost::pfr;

// --- the proposed halp::q15 fixed-point sample type ---
struct q15 {
  std::int16_t v{};
  friend q15 operator*(q15 a, q15 b) noexcept {
    return { std::int16_t((std::int32_t(a.v) * b.v) >> 15) };
  }
};

// --- "ports": each is a struct, exactly like avendish ---
struct audio_sample { q15 sample; };
struct knob         { q15 value; };
template <typename> constexpr bool is_audio = false;
template <> constexpr bool is_audio<audio_sample> = true;

// --- a user processor written avendish-style (aggregate + operator()) ---
struct Gain {
  struct { audio_sample audio; knob gain; } inputs;
  struct { audio_sample audio;            } outputs;
  void operator()() noexcept {
    outputs.audio.sample = inputs.audio.sample * inputs.gain.value;
  }
};

// --- generic, pfr-driven per-sample dispatch (mirrors per_sample_port.hpp) ---
template <typename Fx>
static __always_inline std::int16_t process_one(Fx& fx, std::int16_t in_raw) noexcept {
  pfr::for_each_field(fx.inputs,  [&](auto& f){ if constexpr (is_audio<std::decay_t<decltype(f)>>) f.sample.v = in_raw; });
  fx();
  std::int16_t out = 0;
  pfr::for_each_field(fx.outputs, [&](auto& f){ if constexpr (is_audio<std::decay_t<decltype(f)>>) out = f.sample.v; });
  return out;
}

extern "C" {
// Control surface: userspace writes the q15 gain into entry 0 (the "knobs in a
// map" design). BTF map declared with explicit field types (no __uint/__type).
struct {
  int    (*type)[BPF_MAP_TYPE_ARRAY];
  int    (*max_entries)[1];
  __u32  *key;
  __s16  *value;
} gain_ctl SEC(".maps");

SEC("xdp")
int xdp_audio_gain(struct xdp_md* ctx) {
  void* data     = (void*)(long)ctx->data;
  void* data_end = (void*)(long)ctx->data_end;

  __u32 k = 0;
  std::int16_t* g = (std::int16_t*)bpf_map_lookup_elem(&gain_ctl, &k);
  if (!g) return XDP_PASS;

  Gain fx{};
  fx.inputs.gain.value.v = *g;

  // Treat the payload as L16 PCM samples; bounded, bounds-checked loop.
  std::int16_t* s = (std::int16_t*)data;
  for (int i = 0; i < 64; ++i) {            // compile-time bound for the verifier
    if ((void*)(s + 1) > data_end) break;   // mandatory packet bounds check
    *s = process_one(fx, *s);
    ++s;
  }
  return XDP_PASS;
}
}  // extern "C"

char _license[] SEC("license") = "GPL";
