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
| Controls: range slider, xyz/xyzw spinboxes, time chooser, string enum, folder, file, multi-slider | ok | ok | ok | ok |
| Sample-accurate control, smooth modifier, soundfile/midifile ports, dynamic port | ok | ok | ok | ok |
| Value I/O float/int/bool/string | ok | ok | ok | ok |
| Messages, callback | ok | ok | ok | ok |
| MIDI passthrough / MIDI out (note generation) | ok | ok | ok | ok |
| Worker (async thread-pool request) | ok | ok | ok | ok |
| Audio: per-sample **ports**, bus (args/fixed/dynamic), per-frame, variable channels | ok | ok | ok | ok |
| Audio: per-sample **args** (`float operator()(float)`) — `TestAudioGainMono` | ok | FAIL | FAIL | FAIL |
| Texture RGBA8 / RGB / variable / generator | ok | ok | — | ok |
| Texture **R32F / RGBA32F** (float) | ok | ok | — | ok |
| Buffer raw | ok | ok | ok | ok |
| Buffer **typed** — `TestBufferTypedIO` | ok | ok | ok | — |
| **GPU buffer** in/out | ok | **FAIL** | ok | — |
| Tensor input | ok | ok | ok | ok |
| Aggregate value (`halp_field_names`) | ok | ok | ok | — |
| Geometry **static** prefab generator | ok | ok | — | ok |
| Geometry **dynamic** (CPU/GPU) filter | ok | ok | — | **FAIL** |
| Metadata, ticks (tick/musical/flicks), lifecycle (prepare/initialize/bypass) | ok | ok | ok | ok |

## Fixed
- **TouchDesigner TOP — float textures** (`R32F`/`RGBA32F`): download path cast `float*` from
  `unsigned char*`; now casts to the texture's actual byte pointer type.
- **Max/Jitter — typed buffers**: `cpu_typed_buffer` required `byte_offset` (absent on
  `typed_buffer`), so `setup` fell through to a deleted overload; added the member and a typed
  `matrix_to_buffer` path (plus a latent `buf.count`→`element_count` typo).
- **Max/Jitter — aggregate value ports**: `from_dict`'s field-named overload took `V&&` and lost
  partial-ordering to the deleted `V&` catch-all; now `V&`, decomposing the struct field-by-field.

## Gaps remaining
- **Per-sample-arg audio** (`float operator()(float)`): fails on Max, Pd, and TD — only the
  per-sample *port* form binds. Poly effect-container replication bug.
- **GPU buffers**: opaque `handle` has no CPU-side path on Max/Pd; needs a real upload/download
  or CPU-backed fallback.
- **TouchDesigner — dynamic geometry**: SOP only reads generators; input-geometry filter path
  is unimplemented (TODO in `sop/geometry_processor.hpp`).
- **TouchDesigner — operator family**: buffer/aggregate objects have no auto processor category;
  needs explicit user selection (`touchdesigner:CHOP`/`SOP`/`DAT`/…).
