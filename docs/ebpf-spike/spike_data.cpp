// Phase-0 spike #2 (data/message/string scope): an avendish-style DATA object
// — int + fixed-capacity string + struct ports, a bounded string compare (the
// canonical eBPF "filter by name" op), pfr-driven field iteration — lowered to
// a loadable XDP classifier. No floating point anywhere.
//
// Build via ./build.sh (it compiles every spike_*.cpp / xdp_*.cpp).
#include <linux/bpf.h>
#include "bpf_cxx.hpp"
#include <boost/pfr/core.hpp>   // structural reflection only
#include <cstdint>

namespace pfr = boost::pfr;

// --- runtime fixed-capacity string: the eBPF-friendly std::string replacement
//     (the gap found in avendish: limited_string<N> is heap-backed; static_string
//     is consteval-only). POD, inline buffer, bounded ops. -> proposed halp type.
template <unsigned N>
struct fixed_string {
  char     data[N]{};
  unsigned len{};
  bool equals(const char* rhs, unsigned rlen) const noexcept {
    if (rlen != len) return false;
    #pragma unroll
    for (unsigned i = 0; i < N; ++i) {     // compile-time bound for the verifier
      if (i >= len) break;
      if (data[i] != rhs[i]) return false;
    }
    return true;
  }
};

// --- avendish-style value port ---
template <typename T> struct val { T value{}; };

// --- a "data object" written avendish-style: classify an event, int/string only
struct Classifier {
  struct {
    val<std::uint32_t>    proto;   // int input port
    val<fixed_string<16>> tag;     // string input port
  } inputs;
  struct {
    val<std::uint32_t>    klass;   // int output port
  } outputs;

  void operator()() noexcept {
    // pure int/string logic: no FP, no alloc, no syscalls
    if (inputs.proto.value == 0x0800u && inputs.tag.value.equals("AUDIO", 5))
      outputs.klass.value = 1;
    else
      outputs.klass.value = 0;
  }
};

extern "C" {
// per-class packet counters (the data sink)
struct {
  int   (*type)[BPF_MAP_TYPE_ARRAY];
  int   (*max_entries)[2];
  __u32 *key;
  __u64 *value;
} class_counts SEC(".maps");

SEC("xdp")
int xdp_classify(struct xdp_md* ctx) {
  unsigned char* data     = (unsigned char*)(long)ctx->data;
  unsigned char* data_end = (unsigned char*)(long)ctx->data_end;
  if (data + 16 > data_end) return XDP_PASS;   // verifier-mandated bounds check

  Classifier fx{};
  // marshal raw event bytes into typed ports (backend-specific wire format)
  fx.inputs.proto.value = ((std::uint32_t)data[12] << 8) | data[13];
  #pragma unroll
  for (int i = 0; i < 5; ++i) fx.inputs.tag.value.data[i] = (char)data[i];
  fx.inputs.tag.value.len = 5;

  fx();   // run the avendish data object

  // pfr-driven generic read of the output port(s)
  __u32 k = 0;
  pfr::for_each_field(fx.outputs, [&](auto& f){ k = f.value; });

  __u64* c = (__u64*)bpf_map_lookup_elem(&class_counts, &k);
  if (c) *c += 1;
  return XDP_PASS;
}
}  // extern "C"

char _license[] SEC("license") = "GPL";
