# Avendish → eBPF and the sandboxed-execution family

> Working plan for a Linux Audio Conference talk + implementation.
> **Scope:** standard **message / data / string / int** objects — *not* audio DSP.
> The kernel-side story is audio *infrastructure* (routing, observability,
> security, scheduling), authored as ordinary avendish data objects.
> Status: **Phase-0 feasibility spikes DONE — R1 retired.** See §0.5 + `docs/ebpf-spike/`.

## 0.5. Phase-0 results — the core risk is retired ✅

The headline unknown (R1: *does avendish's Boost.PFR reflection + value/message
ports lower cleanly to BPF, and coexist with real BPF scaffolding?*) was
validated experimentally. Reproducible: `docs/ebpf-spike/` → `./build.sh`.

**Spike #1 — reflection + scaffolding (`spike_pfr.cpp`, `xdp_gain.cpp`):**
- `boost::pfr::for_each_field` over avendish-style `inputs`/`outputs` structs
  **lowers to direct offset loads/stores — fully zero-cost on BPF.** The
  reflection disappears entirely; the disassembly matches hand-written C BPF.
- A *genuinely loadable* `SEC("xdp")` program compiles with PFR + a
  `BPF_MAP_TYPE_ARRAY` control surface (`bpf_map_lookup_elem`) + the
  verifier-mandated bounds check, producing an object with the right sections
  (`.text`, `xdp`, `.maps`, `.BTF`, `license`) + a working libbpf skeleton.

**Spike #2 — the data/string scope (`spike_data.cpp`): the relevant proof.**
A loadable XDP **classifier** written as an avendish-style data object —
`val<uint32_t>` + `val<fixed_string<16>>` input ports, a `val<uint32_t>` output,
pure int/string `operator()` logic. The disassembly is textbook:
- int marshalling (`(data[12]<<8)|data[13]`) → byte loads + shift/or;
- **`fixed_string::equals("AUDIO", 5)` lowered to fully-unrolled byte compares**
  against immediates `0x41 0x55 0x44 0x49 0x4f` — the canonical eBPF
  "filter-by-name" op, **no loop, no `.rodata`, no heap, no FP**;
- `pfr::for_each_field` over the output port → vanished (zero-cost), value flows
  straight to a map key;
- `bpf_map_lookup_elem` + null-check + `*c += 1` counter increment.

→ **int + fixed-string + struct ports + bounded string ops + pfr dispatch + maps
all lower to clean, verifier-shaped BPF.** This is exactly the target object class.

**Environment:** clang 22 (bpf target), bpftool 7.7, libbpf, Boost.PFR 1.87,
kernel 7.0 + BTF. All present, all modern.

**Three toolchain gotchas found — all surmountable, all solved in the spike:**
1. Cross-targeting `bpf` doesn't add host C++ header paths → discover them from
   the host compiler and inject as `-isystem` (the backend's CMake does this).
2. `<boost/pfr.hpp>` drags in field-*name* reflection → `<string_view>` →
   `<cwchar>` → glibc `floatn.h` → `__float128` (unsupported on BPF). **Fix:
   include only `<boost/pfr/core.hpp>`** (structural reflection — all the BPF
   side needs). The same `<string_view>`/`<vector>` chain is why
   `concepts/message.hpp` can't be dragged whole into the BPF TU — the BPF side
   needs **curated includes + bounded value types** (§3).
3. libbpf's `<bpf/bpf_helpers.h>` isn't C++-clean (`= (void*)N` helper decls;
   `__uint`/`__type` macros collide with libstdc++). **Fix: ship a tiny
   `bpf_cxx.hpp` shim** + declare BTF maps with explicit field types → becomes
   `include/avnd/binding/ebpf/bpf_cxx.hpp`.

**Not yet proven:** in-kernel verifier *acceptance* (needs `CAP_BPF`; the sandbox
returned `-EPERM` at the probe stage — a privilege gate, not a verdict on our
code). Low residual risk: straight-line integer/byte ops, explicit bounds checks,
bounded/unrolled loops. **Action: run the one-line `sudo` load in
`docs/ebpf-spike/README.md` on a dev box.**

**Implication:** **Strategy 1 (direct C++→BPF) is viable** for data/message/string
objects. The transpile fallback (§4) is demoted to a contingency.

---

## 1. The constraint wall — eBPF vs. avendish data objects

eBPF (in-kernel) is a verified, sandboxed ISA. For *data/message/string/int*
objects the binding numeric constraint (no FP) barely matters; the real wall is
**dynamic memory and unbounded data**. Mapped against what avendish does:

| eBPF constraint | Avendish impact | Mitigation / fit |
|---|---|---|
| **No `std::string` / `std::vector` / `std::map` / heap** | Message/data ports use exactly these (`val_port<"x", std::string>`, `std::vector<int>`, `fmt`, `iostream` — see `CompleteMessageExample.hpp`) | **New: bounded runtime value types** — `fixed_string<N>`, `fixed_vector<T,N>` (§3). Unbounded collections live in **BPF maps**, not on the heap. |
| **`limited_string<N>` is heap-backed** (`: std::string`); `static_string<N>` is `consteval`-only | No runtime fixed-capacity string exists yet | Genuine gap → `halp/fixed_string.hpp` (proven in spike #2). |
| **Bounded loops / string ops only** (`bpf_loop()` ≥5.17; `bpf_strncmp` ≥5.17; else unroll) | Arbitrary `std::string` ops, ranged `for` over vectors | Bounded ops with a compile-time cap: `equals/copy/hash` over `fixed_string<N>`. Proven: `equals()` → unrolled byte compares. |
| **512-byte stack** | Big `inputs`/`outputs` structs (long strings, many ports) overflow it | Keep large/stateful data in maps; `static_assert(sizeof(ports) <= cap)`. |
| **Event-driven, run-to-completion** (attached to a hook, fires per event) | Message objects are **already event-driven** (`operator()`, `messages{}` handlers) | *Best fit of any avendish target.* A backend shim maps "one hook firing" → "marshal event into ports → run handler → emit outputs." |
| **Memory only via maps & verified pointers** | Inputs normally set by a host API; outputs via callbacks | **Inputs ← maps/event bytes; outputs → ringbuf/maps.** avendish's existing thread-safe **message bus** (UI↔audio, "even over the network") maps 1:1 onto **BPF ringbuf** (kernel→userspace). |
| **No exceptions / no RTTI / no libc** | Already solved: `DisableExceptions` target | Reuse it; add `-target bpf -ffreestanding` + curated includes. |
| **No floating point** | Mostly irrelevant for int/string/data objects | If a value port needs non-int math, reuse the `q15`/`q31` fixed type (validated in spike #1) — optional, not central. |

**Key insight:** an avendish **message object is already an event handler with
typed inputs/outputs** — structurally the same thing as a BPF program. The gap is
small and concrete: *(1) bounded value types, (2) curated BPF-side includes,
(3) an event-marshalling shim, (4) map/ringbuf I/O.* No FP work required.

---

## 2. Target taxonomy — kinds of data objects we can target

### 2.1 In-kernel, verified (the real eBPF) — re-ranked for data/message objects

| Target | What an avendish data object becomes | Data domain | LAC wow |
|---|---|---|---|
| **Tracing** (kprobe / tracepoint / USDT) | An **observability object** over PipeWire / JACK / ALSA: count xruns, measure callback jitter, watch `SCHED_FIFO` preemption, aggregate per-client stats → ringbuf to a live UI. The quintessential eBPF use case, authored avendish-style. | int counters, `comm` strings, struct events | ★★★★★ |
| **XDP / TC** (NIC driver hook) | A **packet classifier / router / filter**: parse AES67/RTP/PTP **headers** (not the PCM) — match stream id, route by SSRC, drop malformed, count per-flow, mirror — at line rate. *Touches metadata, ints, byte-strings; no DSP.* | int/byte headers, `fixed_string` ids | ★★★★★ (proven loadable, spike #2) |
| **BPF-LSM** (kernel ≥5.7, `lsm=bpf`) | A **policy object**: "only these apps may open `/dev/snd/*` / grab the RT priority" — allow/deny on string paths + ints. | path strings, uids, ints | ★★★★ |
| **HID-BPF** (kernel ≥6.3) | A **device-data remapper**: rewrite controller/HID reports (incl. USB-MIDI) in the kernel driver — transpose, filter, relabel. Pure byte/int data. | byte/int reports | ★★★★ (live-demoable) |
| **sched_ext** (kernel ≥6.12) | An **audio-aware scheduler** as a data object: prioritize the RT audio thread so it never misses its deadline. | task structs, ints | ★★★★★ (most novel) |
| **socket / cgroup / sockops** | Per-app audio-stream policing, QoS marking | ints, byte-strings | ★★★☆☆ |

### 2.2 Userspace eBPF — the "have your cake" lane

| Target | Why it matters |
|---|---|
| **bpftime** (userspace eBPF VM, LLVM-JIT) | Far more permissive than the kernel verifier (FP, larger programs, `std`-ish). The *same bytecode* runs in-kernel (restricted) or in userspace (full). De-risks demos and proves "one source, two VMs." **uprobe** hooks → instrument any audio app *without recompiling it*. |

### 2.3 The wider sandboxed/verified family — the "and similar targets"

| Target | Status / note |
|---|---|
| **WASM** | ✅ already implemented (`binding/wasm/`) — the precedent that proves the model. Open with it. |
| **RISC-V / RP2040 / ESP32** | Partial; the bounded-value-type + freestanding work here helps directly. |
| **SPIR-V / WGSL (GPU compute)** | ✅ already done via `gpp` + Qt RHI. Adjacent verified-ISA story. |
| **P4** (programmable switch data planes) | Audio-stream routing/classification in a switch ASIC. Adjacent to XDP. |
| **Cranelift / LLVM-ORC JIT** | Runtime codegen / hot-reload of avendish data objects. |
| **Solana SBF** | Solana's VM is a fork of eBPF (rBPF/SBF). "Your data object, as a smart contract." Closing gag — provocative, memorable. |

### 2.4 Ranking (novelty × feasibility × LAC audience)

1. **Tracing object over PipeWire/JACK** — headline. High wow, definitely works, pure int/string data.
2. **XDP AES67-header classifier/router** — proven loadable (spike #2); great visual (live per-flow counters).
3. **sched_ext audio scheduler** — most novel idea in the room.
4. **HID-BPF report remapper** — small, live-demoable with real hardware.
5. **BPF-LSM audio-device policy** + **bpftime uprobe** — strong supporting acts.

---

## 3. Framework change #1 — bounded runtime value types (the core enabler)

The data path uses `std::string` / `std::vector` / custom aggregates. We need
heap-free, fixed-capacity, POD equivalents so value/message ports compile to BPF.
Spike #2 already validated `fixed_string<N>`.

```cpp
// halp/fixed_string.hpp (new) — runtime, POD, inline buffer, bounded ops
namespace halp {
template <unsigned N>
struct fixed_string {
  char     data[N]{};
  unsigned len{};
  bool equals(const char* s, unsigned n) const noexcept;   // -> unrolled byte cmp
  void assign(const char* s, unsigned n) noexcept;          // bounded copy
  // (consteval ctor from string literal for parity with static_string)
};

// halp/fixed_vector.hpp (new) — fixed-capacity inline array
template <typename T, unsigned N>
struct fixed_vector { T data[N]{}; unsigned size{}; /* bounded push/iterate */ };
}
```

Design goal: **the same processor source compiles for host and BPF** by choosing
the port's storage type. A `val_port<"tag", std::string>` (host) ↔
`val_port<"tag", halp::fixed_string<16>>` (BPF) — ideally selectable via a config
trait so one struct serves both, mirroring how the float/`q15` substitution works.

Tasks:
- `halp/fixed_string.hpp`, `halp/fixed_vector.hpp` — POD, bounded ops, the
  `string_ish` / container concepts in `concepts/` taught to accept them.
- A **bounded-value-type concept** so introspection (`val_port`, message args)
  can constrain to BPF-safe types and reject `std::string`/`std::vector` at
  compile time with a clear error on the BPF path.
- These are **independently useful** (ESP32, lock-free message bus, serialization)
  → easy to upstream, not eBPF-only.
- *(Optional)* reuse `q15`/`q31` from spike #1 if any value port needs non-int math.

---

## 4. Framework change #2 — the `bpf` build config & codegen strategy

### Build config (`cmake/avendish.ebpf.cmake`, mirrors `disableexceptions`)
- `-target bpf -O2 -g` (BTF), `-ffreestanding`, `-fno-exceptions -fno-rtti`,
  `-fno-stack-protector`; **curated includes** (pfr-core + bounded value types,
  *not* `<string_view>`/`<vector>`); inject host C++ header dirs as `-isystem`.
- Self-skip (like every backend) if clang-bpf / libbpf / bpftool absent. Add
  `option(AVND_ENABLE_EBPF ... ON)`.

### Codegen strategies
1. **Direct (confirmed viable, the plan):** compile the data object's C++ straight
   to BPF with `clang -target bpf`. Proven by both spikes.
2. **Transpile (contingency):** emit restricted C from introspection (like the
   `dump` backend) for any object the verifier rejects. Now a fallback, not the plan.
3. **bpftime (FP / heavy lane + safety net):** userspace VM for objects that need
   FP or exceed verifier limits.

---

## 5. The backend — `include/avnd/binding/ebpf/`

Mirror the anatomy of `binding/example/example_processor.hpp` and
`binding/wasm/audio_processor.hpp`. The conceptual core is the **event shim**:
*one hook firing → marshal the event into the object's input ports → run
`operator()` / the matching `messages{}` handler → emit outputs to ringbuf/maps.*

```
include/avnd/binding/ebpf/
  bpf_cxx.hpp          # C++-clean SEC/helpers/map shim (from spike; DONE)
  configure.hpp        # config: bounded value types, no fmt/iostream, freestanding
  value_marshal.hpp    # event bytes <-> typed ports (per-hook wire format)
  maps.hpp             # BPF map + ringbuf layout from port introspection
  kprobe_processor.hpp # kprobe/tracepoint ctx -> message/data object  (CO-RE/BTF)
  xdp_processor.hpp    # XDP/TC ctx -> packet classifier/router        (UAPI, no vmlinux.h)
  lsm_processor.hpp    # LSM hook ctx -> allow/deny policy object
  hid_processor.hpp    # HID-BPF report in/out
  prototype.bpf.c.in   # generated TU compiled with clang -target bpf
  loader/              # libbpf loader + ringbuf drain + control surface
                       #   (itself an avendish standalone object → "the UI is avendish too")
  all.hpp
```

```cpp
// xdp_processor.hpp (sketch) — matches the proven spike_data.cpp shape
template <typename T>
struct xdp_processor {
  avnd::effect_container<T> object;
  int on_event(struct xdp_md* ctx) {
    if (!bounds_ok(ctx)) return XDP_PASS;
    marshal_inputs(object, ctx);            // event bytes -> typed ports (pfr-driven)
    avnd::process_message(object);          // run operator()/messages handler
    emit_outputs(object, /*ringbuf,*/ ctx); // output ports -> ringbuf/maps
    return decide(object);                  // e.g. XDP_PASS / XDP_DROP from an output port
  }
};
```

**Note (real risk):** kprobe/tracepoint marshalling reads kernel structs → needs
CO-RE + `vmlinux.h`, which is C and awkward from C++. XDP (UAPI `xdp_md`) and
HID-BPF avoid this. Plan: lead with XDP/HID; for tracing, isolate kernel-struct
reads behind a small C shim or use raw_tracepoint with explicit offsets.

---

## 6. CMake registration — `cmake/avendish.ebpf.cmake`

Follow the established pattern (`avendish.wasm.cmake` is the closest analog):
- `function(avnd_make_ebpf)`: compile `prototype.bpf.c` → `.bpf.o`
  (`clang -target bpf`), `bpftool gen skeleton` → `.skel.h`, build the libbpf
  loader, link `libbpf`.
- Register in `cmake/avendish.cmake`: `option(AVND_ENABLE_EBPF "Enable eBPF backend" ON)`,
  `include(avendish.ebpf)`.
- **Do not** add to `avnd_make_all`/`avnd_make_object` by default (breaks builds
  without clang-bpf). Add a dedicated preset:

```cmake
function(avnd_make_ebpf_object)
  avnd_register(${ARGV})
  avnd_make_dump(${ARGV})                                   # always works
  _avnd_dispatch_backend(ebpf ${ARGV} PROCESSOR_TYPE XDP)   # gated by AVND_ENABLE_EBPF
endfunction()
```

(`PROCESSOR_TYPE KPROBE|TRACEPOINT|XDP|LSM|HID|SCHED`, like other backends pass
`CHOP_AUDIO`/`TOP`.)

---

## 7. Phased plan

| Phase | Goal | Exit criterion |
|---|---|---|
| **0. Spike** | ✅ **DONE** — PFR + int/string/struct ports + map → loadable XDP classifier (§0.5, `docs/ebpf-spike/`) | ✅ Strategy 1 viable. Remaining: run `sudo` load for the verifier verdict. |
| **1. Value types** | `halp/fixed_string.hpp`, `halp/fixed_vector.hpp` + bounded-value concept | A `val_port`/`messages` data object built with them compiles in the example host *and* to `.bpf.o` |
| **2. Build config** | `avendish.ebpf.cmake`, `AVND_ENABLE_EBPF`, curated includes, self-skip | An avendish data object compiles to a `.bpf.o` that `bpftool` loads + skeleton |
| **3. XDP backend** | `xdp_processor.hpp` + `value_marshal` + maps + ringbuf + loader | AES67-header classifier on a veth pair, live per-flow counters from a userspace UI |
| **4. Tracing backend** | `kprobe_processor.hpp` (CO-RE) | PipeWire/JACK observability object → ringbuf → live UI |
| **5. LSM / HID** | `lsm_processor.hpp`, `hid_processor.hpp` | audio-device policy object; HID report remap on real hardware |
| **6. Stretch** | sched_ext; bpftime uprobe | Demo-only, optional |

### Risk register
- **R1 (~~high~~ → RESOLVED): C++→BPF lowering.** ✅ Both spikes prove PFR +
  int/string/struct ports + maps lower to zero-cost, verifier-shaped BPF. Only
  the in-kernel verifier verdict remains (low; one `sudo` load away).
- **R2 (med): kernel-struct marshalling from C++** (kprobe/tracepoint need
  `vmlinux.h`/CO-RE, awkward in C++). Mitigation: lead with XDP/HID (UAPI only);
  isolate kernel reads behind a C shim for tracing.
- **R3 (med): bounded-loop / verifier rejection on string-heavy logic.**
  Mitigation: compile-time caps, `#pragma unroll` / `bpf_loop()`, `bpf_strncmp`.
- **R4 (low): map/ringbuf I/O ergonomics** vs. avendish's callback model.
  Mitigation: map the message-bus/callback concept onto ringbuf events.

---

## 8. Demo arc for the talk (proposed)

1. **Open:** show a small avendish **message/data object**. "This is a Max object.
   And a Pd object. And Python. And WASM." (the known avendish magic).
2. **Turn:** "What's the most hostile host we could give it? The kernel."
3. **Headline:** the *same* object as a **PipeWire/JACK observability tracer** —
   counting xruns / jitter in-kernel, streaming to a live UI (itself an avendish
   standalone object). Then the **XDP AES67-header classifier** with live per-flow
   counters (the proven spike).
4. **Mind-bender:** **sched_ext** — "we wrote a Linux scheduler from the same
   framework you write Max objects in," or **BPF-LSM** audio-device policy.
5. **Bridge:** **bpftime** — the same object, now a uprobe instrumenting an
   unmodified DAW. "One source, two VMs."
6. **Closing gag:** the **Solana SBF** slide. "Yes, it also runs as a smart
   contract. No, you should not do this."

---

## 9. Open questions (need a decision before Phase 1)

- **Demo anchor:** lead with the tracing object (definitely works) or the XDP
  classifier (already proven loadable, more visual)?
- **One-source-two-storage:** make port storage type (`std::string` ↔
  `fixed_string<N>`) selectable via a config trait so a single struct serves both
  host and BPF — or accept a BPF-specific variant of the object?
- **Tracing marshalling:** invest in CO-RE/`vmlinux.h`-from-C++ now, or ship a
  small C marshalling shim and keep the C++ side pure?
- **Upstream framing:** land `fixed_string`/`fixed_vector` + freestanding as
  generally-useful PRs first (they help ESP32 + the message bus too), keep the
  eBPF backend as a showcase branch?
