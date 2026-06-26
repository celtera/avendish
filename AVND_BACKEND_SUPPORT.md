# Backend feature support (empirical, compile-level)

Each test object in `examples/Tests/` isolates one feature. This matrix is produced
by compiling every object's generated per-backend binding TU (`tooling/backend_support_matrix.ps1`).

- `ok` — avendish generates a binding that compiles
- `FAIL` — the binding fails to compile (feature unsupported on that backend)
- `—` — not registered for that backend

`dump` / `max` / `pd` / `td` / `py` are verified locally (clang-cl + pybind11). godot / wasm /
vst3 / clap / ossia / gstreamer are exercised by avendish CI, not per-object here.

¹ TD has no automatic operator family for some port shapes (buffers, aggregates). Pick one
explicitly: `BACKENDS … touchdesigner:CHOP_MESSAGE` (also `:TOP`/`:CHOP_AUDIO`/`:SOP`/`:POP`/`:DAT`).
Objects with no chosen family are skipped on TD with a status message rather than erroring.

² `py` = exposed and driven by `tooling/test_avnd_python.py` (numpy). Ports map to attributes;
audio runs via `obj.process_audio(ndarray)`. `~` = compiles/runs but the port is not marshaled
to Python yet (geometry buffer layout; soundfile/midifile decoding is host-loaded).

| Feature (object) | dump | max | pd | td | py² |
|---|---|---|---|---|---|
| Controls: float/int slider, knob, spinbox, toggle, lineedit, buttons, enum, xy, color, bargraph | ok | ok | ok | ok | ok |
| Controls: range slider, xyz/xyzw spinboxes, time chooser, string enum, folder, file, multi-slider | ok | ok | ok | ok | ok |
| Sample-accurate control, smooth modifier, soundfile/midifile ports, dynamic port | ok | ok | ok | ok | ok |
| Value I/O float/int/bool/string | ok | ok | ok | ok | ok |
| Messages, callback | ok | ok | ok | ok | ok |
| MIDI passthrough / MIDI out (note generation) | ok | ok | ok | ok | ok |
| Worker (async thread-pool request) | ok | ok | ok | ok | ok |
| Audio: per-sample **ports**, bus (args/fixed/dynamic), per-frame, variable channels | ok | ok | ok | ok | ok |
| Audio: per-sample **args** (`float operator()(float)`) — `TestAudioGainMono` | ok | ok | ok | ok | ok |
| Texture RGBA8 / RGB / variable / generator | ok | ok | — | ok | ok |
| Texture **R32F / RGBA32F** (float) | ok | ok | — | ok | ok |
| Buffer raw | ok | ok | ok | ok | ok |
| Buffer **typed** — `TestBufferTypedIO` | ok | ok | ok | ok¹ | ok |
| **GPU buffer** in/out | ok | ok | ok | — | ok |
| Tensor input | ok | ok | ok | ok | ok |
| Aggregate value (`halp_field_names`) | ok | ok | ok | ok¹ | ok |
| Geometry **static** prefab generator | ok | ok | — | ok | ~ |
| Geometry **dynamic** (CPU/GPU) filter | ok | ok | — | ok | ~ |
| Metadata, ticks (tick/musical/flicks), lifecycle (prepare/initialize/bypass) | ok | ok | ok | ok | ok |

## Fixed
- **TouchDesigner TOP — float textures** (`R32F`/`RGBA32F`): download path cast `float*` from
  `unsigned char*`; now casts to the texture's actual byte pointer type.
- **Max/Jitter — typed buffers**: `cpu_typed_buffer` required `byte_offset` (absent on
  `typed_buffer`), so `setup` fell through to a deleted overload; added the member and a typed
  `matrix_to_buffer` path (plus a latent `buf.count`→`element_count` typo).
- **Max/Jitter — aggregate value ports**: `from_dict`'s field-named overload took `V&&` and lost
  partial-ordering to the deleted `V&` catch-all; now `V&`, decomposing the struct field-by-field.
- **Per-sample-arg audio** (`float operator()(float)`): the monophonic multi-instance
  effect-container's `full_state(i)` returned `effect[i].effect` (no such member); now `effect[i]`.
  Max/Pd `dumpall` helper also handled the multi-instance inputs range.
- **TD operator-family selection**: `avnd_make(BACKENDS …)` now accepts `touchdesigner:<FAMILY>`
  to pick TOP/CHOP_AUDIO/CHOP_MESSAGE/SOP/POP/DAT; objects with no family are skipped (status
  message) instead of erroring. Buffers/aggregates bind as CHOP_MESSAGE (fixed a `buf.count` typo).

- **GPU buffers**: `gpu_buffer_output` now owns host storage (`allocate(bytes)`); the Max jitter
  path moves the bytes through a char matrix (handle = host pointer on CPU hosts). GPU-native
  hosts (ossia/score, TD POP device-memory) still use their own GPU handles.
- **TD dynamic geometry filters**: SOP/POP hardcoded a `.mesh` member; geometry ports can also
  be the geometry directly. Added a mesh resolver and a functional SOP input reader
  (point position/normal/color) so CPU `dynamic_geometry` filters work, not just generators.

All seven gaps from the original audit are closed for the locally-verifiable backends
(dump/max/pd/td). GPU-native data transfer on GPU-capable hosts remains host-managed by design.

## Python backend

Previously a control/message/tensor-only binding. Extended so objects can be driven as
complete unit-test harnesses from Python (`include/avnd/binding/python/`):

- **Audio** — `obj.process_audio(ndarray[channels, frames])` runs the process adapter over a
  block (`audio.hpp`); all forms (per-sample arg/port, bus, frame, poly) verified.
- **Textures** — CPU texture ports as `(H, W, C)` numpy (uint8 / float32 by format).
- **Buffers** — raw/gpu as uint8 bytes, typed as element dtype; input buffers kept alive via
  per-instance dynamic attributes.
- **MIDI** — list of `(bytes, timestamp)`.
- **Dynamic ports** — list of sub-port values (assigning resizes).
- **File ports** — contents as Python `bytes`.

Build a module (pybind11 + Python dev headers required) and run the harness:

```
# CMake finds pybind11 via CMAKE_PREFIX_PATH (e.g. `pip install pybind11`):
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(python -m pybind11 --cmakedir)"
cmake --build build --target <Object>_python
PYTHONPATH=build:tooling python tooling/test_avnd_python.py   # 17 cases, all port types
```

Not yet marshaled to Python: geometry buffer layout, and soundfile/midifile sample decoding
(host-loaded). `inputs_is_type` audio objects expose `process_audio` but not their controls.
