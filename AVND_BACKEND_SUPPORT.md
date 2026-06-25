# Backend feature support (empirical, compile-level)

Each test object in `examples/Tests/` isolates one feature. This matrix is produced
by compiling every object's generated per-backend binding TU (`tooling/backend_support_matrix.ps1`).

- `ok` — avendish generates a binding that compiles
- `FAIL` — the binding fails to compile (feature unsupported on that backend)
- `—` — not registered for that backend

`dump` / `max` / `pd` / `td` are verified locally (clang-cl). godot / python / wasm /
vst3 / clap / ossia / gstreamer are exercised by avendish CI, not per-object here.

| Feature (object) | dump | max | pd | td |
|---|---|---|---|---|
| Controls: float/int slider, knob, spinbox, toggle, lineedit, buttons, enum, xy, color, bargraph | ok | ok | ok | ok |
| Value I/O float/int/bool/string | ok | ok | ok | ok |
| Messages, callback | ok | ok | ok | ok |
| MIDI passthrough | ok | ok | ok | ok |
| Audio: per-sample **ports**, bus (args/fixed/dynamic), per-frame, variable channels | ok | ok | ok | ok |
| Audio: per-sample **args** (`float operator()(float)`) — `TestAudioGainMono` | ok | FAIL | FAIL | FAIL |
| Texture RGBA8 / RGB / variable / generator | ok | ok | — | ok |
| Texture **R32F / RGBA32F** (float) | ok | ok | — | **FAIL** |
| Buffer raw | ok | ok | ok | ok |
| Buffer **typed** — `TestBufferTypedIO` | ok | **FAIL** | ok | ok |
| **GPU buffer** in/out | ok | **FAIL** | ok | ok |
| Tensor input | ok | ok | ok | ok |
| Aggregate value (`halp_field_names`) | ok | **FAIL** | ok | ok |
| Geometry **static** prefab generator | ok | ok | — | ok |
| Geometry **dynamic** (CPU/GPU) filter | ok | ok | — | **FAIL** |
| Metadata, ticks (tick/musical/flicks), lifecycle (prepare/initialize/bypass) | ok | ok | ok | ok |

## Gaps found
- **Per-sample-arg audio** (`float operator()(float)`): fails on Max, Pd, and TD — only the
  per-sample *port* form (`audio_sample<>` in `inputs`/`outputs` types) binds on the patch
  backends.
- **TouchDesigner TOP — float texture formats**: `R32F` / `RGBA32F` fail to compile; 8-bit
  RGBA/RGB are fine. (TD `OP_PixelFormat` float-format mapping path.)
- **TouchDesigner — dynamic geometry**: `dynamic_geometry` / `dynamic_gpu_geometry` in+out
  filters fail; only static-prefab geometry *generators* (SOP) compile.
- **Max/Jitter**: typed buffers, GPU buffers, and aggregate value ports fail (the jitter
  binding assumes char/raw matrix data; `gpu_buffer` has no `raw_data`, etc.).

These are candidate follow-up fixes in the respective avendish bindings; for now the affected
objects are registered only for the backends where they compile.
