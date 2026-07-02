# Output verification plan — golden differential testing

Goal: verify every avendish object actually **cooks** and produces **correct
output** given deterministic test input, across all backends. The oracle is the
**raw C++ object** run in-process (the same wrappers the bindings use); every
backend must reproduce its output.

## Why this design

- The `avnd_test_*` objects are synthetic — there is no external spec for
  "correct". The raw object is deterministic and *is* the contract every binding
  implements, so it is the natural oracle.
- Running the raw object also directly answers "does it cook and produce
  output?" for the reference.
- Differential comparison against the raw object is the **data-level** version of
  the crash sweeps: it catches bindings that produce *wrong values*, not just
  crashes. (The Max #127 / GStreamer #130 / TD #132 channel bugs would all show
  as wrong/short output here too.)

## Key artifact: the golden JSON

One file per object, `golden/<c_name>.json`, containing **inputs, outputs, and
metadata** so backend harnesses are simple "apply the recorded inputs, read the
outputs, diff":

```jsonc
{
  "c_name": "avnd_lowpass",
  "meta": {
    "seed": 1234, "frames": 64, "rate": 44100,
    "compare": "exact",          // or "structural" (shape/range only)
    "families": ["CHOP"]         // TD/backend hint
  },
  "inputs": {
    "controls": { "cutoff": 0.5, "resonance": 0.2 },
    "audio":    { "in":  [[/*ch0 samples*/], [/*ch1*/]] },
    "messages": [ { "name": "reset", "args": [] } ]
  },
  "outputs": {
    "controls": { "peak": 0.71 },
    "audio":    { "out": [[/*ch0*/], [/*ch1*/]] },   // or {hash, first_n}
    "texture":  { "width": 16, "height": 16, "format": "rgba8", "hash": "…" },
    "dat":      [["a","b"],["c","d"]]
  }
}
```

Inputs are recorded so every backend feeds *exactly* what the oracle used —
input generation lives in one place (the generator), backends stay dumb.

## Deterministic input policy (in the generator, recorded to JSON)

- **controls**: a fixed function of type/range — floats → `min + 0.5*(max-min)`
  (or 0.5), ints → mid, bool → true, enum → index 1, string → "test".
- **audio inputs**: a known signal per channel — e.g. `sin(2π·(k+1)·i/frames)`
  for channel k (distinct per channel, bounded), seeded.
- **messages**: call each message once with fixed args derived from its arg
  types.

## Determinism strategy

- `compare: "exact"` (float tol ~1e-5) for pure/stateless objects
  (gain, arithmetic, filters).
- `compare: "structural"` for random / stateful / time-dependent objects
  (noise, oscillators, `tick`): check channel/sample counts, finiteness, and
  value range rather than exact samples. Chosen per object (heuristic: object has
  an RNG / reads time / has persistent state → structural; override via metadata).
- Same seed + rate + frame count on the oracle and every backend.

---

## Phase 1 — Golden generator (ALL objects)  ← first

Mirror the `*_dump` target machinery exactly.

1. `include/avnd/binding/golden/run.hpp` — `template<typename T> ... run(path)`:
   instantiate via `avnd::effect_container<T>` + `avnd::process_adapter<T>`;
   set controls (introspection), fill audio inputs, `prepare`, `process(frames)`,
   send messages; capture all outputs; write `inputs`+`outputs`+`meta` JSON
   (reuse `dump/json_writer.hpp`).
2. `include/avnd/binding/golden/prototype.cpp.in` — like the dump prototype:
   `golden::run<type>(argv[1])`.
3. `cmake/avendish.golden.cmake` — `avnd_make_golden` (mirror `avnd_make_dump`):
   per-object executable → runs → `golden/<c_name>.json`; store
   `AVND_GOLDEN_PATH`; shared PCH; `Avendish::Avendish_golden` interface lib.
4. Dispatch from `avendish.cmake` next to `avnd_make_dump(${ARGV})` so every
   object gets a golden target; add a `goldens` aggregate target.
5. Run across all objects → the golden set. **Deliverable: proof every object
   cooks + produces output for the reference, and the oracle files.**

Edge cases: objects needing GPU / files / network → mark `compare: "skip"` with a
reason (still record that it was skipped, no silent gaps).

## Phase 2 — TouchDesigner end-to-end

Extend `tooling/td/` (the working sweep):

1. `td_run_all.py`: for each object, **load `golden/<c_name>.json`**, apply the
   recorded control values to `n.par`, and feed the recorded audio input by
   wiring a source that reproduces it — a **Script CHOP** (or Constant/Pattern)
   filling the exact sample values; generators need no input.
2. Cook, then **read outputs**: CHOP `n[chan][sample]`, TOP via `n.numpyArray()`
   (hash), DAT `n[r,c]`.
3. Compare to the golden `outputs` within tolerance (or structural). Report per
   object: `cooked / matches / mismatch(Δ) / skipped`.
4. Extend `run_td_sweep.py` summary with pass/mismatch counts; keep the
   crash-resilient relaunch loop.

## Phase 3 — Other backends (easiest → hardest)

1. **Python** (easiest — direct in-process): the python binding is callable from
   Python; the existing unit-test harness already drives ports. Feed golden
   inputs, read outputs, diff. Fast, no GUI.
2. **GStreamer**: `appsrc` (push the golden input buffer) → element → `appsink`
   (pull output) → diff. Headless, deterministic — natural fit (clang64 build).
3. **Pd**: drive via libpd / the test patch; capture audio with `[snapshot~]` /
   write to array; controls via receive. Compare.
4. **Max**: `.maxpat` test harness; capture via `js`/`[capture~]`; compare.
   (No headless — reuse the process-alive + file-drop pattern.)
5. **ossia / vst3 / clap / others**: apply the same golden where a host exists.

Each backend: read golden `inputs` → apply → cook → read outputs → diff golden
`outputs`. Shared comparison helper (tolerance + structural) lives in one place.

## Shared components

- **Comparison** (`tooling/golden_compare.py` or in each harness): exact within
  `abs(a-b) <= atol + rtol*abs(b)`; structural = counts + finiteness + range.
- **Golden schema** documented above; versioned via `meta`.
- **No silent gaps**: skipped/structural objects are logged with the reason.

## Ordering (per request)

1. Phase 1 — golden generator for all objects.
2. Phase 2 — TD end-to-end.
3. Phase 3 — Python → GStreamer → Pd → Max → rest.

Each phase is independently useful and lands as its own PR.
