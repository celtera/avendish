# Phase-0 eBPF feasibility spike

Goal: retire **R1** from `docs/plan-ebpf-target.md` — *does avendish's
compile-time reflection (Boost.PFR) lower cleanly to BPF bytecode, and can it
coexist with real BPF scaffolding (maps, helpers, packet bounds checks)?*

**Verdict: YES.** Run `./build.sh`.

## What it proves

**Primary scope = data / message / string / int objects** (not audio DSP):

- `spike_data.cpp` — **the relevant one.** A loadable XDP **classifier** written
  as an avendish-style data object: `val<uint32_t>` + `val<fixed_string<16>>`
  input ports, a `val<uint32_t>` output, pure int/string `operator()` logic.
  Disassembly: int marshalling → byte loads + shift/or; **`fixed_string::equals
  ("AUDIO",5)` → fully-unrolled byte compares** against immediates (the canonical
  eBPF filter-by-name op, no loop, no `.rodata`, no heap, no FP); pfr field
  iteration → zero-cost; `bpf_map_lookup_elem` + counter increment.

Supporting (proved the reflection mechanics + scaffolding, originally for audio):

- `spike_pfr.cpp` — a `boost::pfr::for_each_field` dispatch compiled to BPF; the
  reflection lowers to **direct offset loads/stores (zero-cost)**.
- `xdp_gain.cpp` — a loadable `SEC("xdp")` program with PFR + a `q15` fixed-point
  type + a `BPF_MAP_TYPE_ARRAY` control + bounds check (kept as evidence the
  numeric/fixed-point path also works, should a value port ever need it).
- `bpf_cxx.hpp` — the small C++-clean BPF shim the backend must ship (see below).

## The three toolchain gotchas (all surmountable, all handled here)

1. **`bpf` target doesn't add host C++ header paths.** Discover them from the
   host compiler and pass as `-isystem` (see `build.sh`).
2. **`<boost/pfr.hpp>` pulls field-*name* reflection → `<string_view>` →
   `<cwchar>` → glibc `floatn.h` → `__float128` (unsupported on BPF).**
   Fix: include only `<boost/pfr/core.hpp>` (structural reflection — all the hot
   path needs). Field names are never needed at the BPF boundary.
3. **libbpf's `<bpf/bpf_helpers.h>` is not C++-clean** — helpers declared
   `= (void*)N` (bad implicit conversion) and `__uint`/`__type` macros collide
   with libstdc++ internals. Fix: ship `bpf_cxx.hpp` and declare BTF maps with
   explicit field types.

## What it could NOT prove here

The final in-kernel **verifier acceptance**. Loading needs `CAP_BPF`, absent in
this sandbox (`bpftool prog loadall` returned `-EPERM` at the *probe* stage —
a privilege gate, not a verdict on our bytecode). To get the real verdict:

```sh
# the data/string classifier (primary scope)
sudo bpftool prog loadall spike_data.bpf.o /sys/fs/bpf/classify
# (or the fixed-point gain, kept as numeric-path evidence)
sudo bpftool prog loadall xdp_gain.bpf.o /sys/fs/bpf/xdp_gain
```

The generated code is straight-line integer ops with explicit bounds checks and
a bounded loop — the shape the verifier is happiest with — so the residual risk
is low.

## Environment used

clang 22.1.6 (bpf target), llvm-objdump 22, bpftool 7.7.0, libbpf headers,
Boost.PFR 1.87, kernel 7.0.11 with BTF at `/sys/kernel/btf/vmlinux`.
