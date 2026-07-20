# Host state (session persistence) support

Status: proposal + reference implementation on `feature/state`.

## Problem

Processors with meaningful state beyond their parameter ports (pattern data,
sample buffers, generated content -- sometimes hundreds of kilobytes) have
no way to persist that state in DAW sessions. Today:

- **VST3**: `Component::getState/setState` serialize exactly one double per
  parameter port (and `Controller::setComponentState` re-reads the same
  doubles). Nothing else is persisted.
- **CLAP**: the binding does not implement `CLAP_EXT_STATE` at all — hosts
  that persist via the state extension (Bitwig, Reaper, qtractor…) save
  *nothing*, not even parameters.
- **vintage (VST2)**: parameters only (host-driven `getParameter` loops);
  `effGetChunk/effSetChunk` are not implemented.

## How Avendish introduces features (per `book/src/development/new_concepts.md`
and the worker / message-bus / programs precedents)

1. prototype the user-facing API as a plain C++ example;
2. declare a **concept** in `include/avnd/concepts/<feature>.hpp` that
   detects the API purely through `requires` clauses — multiple accepted
   spellings, no base classes, no macros;
3. provide access helpers (in `include/avnd/wrappers/` for object-level
   features, `include/avnd/introspection/` for port-level ones) that
   normalize the accepted spellings behind one call;
4. bindings consume the feature behind `if constexpr(avnd::the_concept<T>)`;
   zero cost for objects that don't opt in;
5. static concept tests + Catch2 runtime tests wired in
   `cmake/avendish.tests.cmake`.

This proposal follows that shape exactly.

## Design

### Tier 0 — implicit parameter state (exists, extended)

Bindings keep persisting parameter values as they do today. The CLAP binding
gains this tier as part of this work (it had nothing).

### Tier 1 — opaque custom state (new)

The processor owns an opaque, versioned byte blob; Avendish moves bytes and
never interprets them. **Versioning, migration and validation are entirely
the processor's responsibility** (the blob will come back from arbitrary,
possibly hostile session files -- processors should version, checksum and
sanitize their format).

Accepted spellings, detected by `avnd::has_custom_state<T>`
(= `has_save_state<T> && has_load_state<T>`):

Save — either:
```cpp
// (a) container return: any contiguous byte container (vector<uint8_t>,
//     vector<char>, string, vector<std::byte>, ...)
std::vector<uint8_t> save_state();

// (b) writer callback: zero-allocation streaming; the writer may be invoked
//     any number of times, bytes are concatenated.
void save_state(avnd::state_writer auto& write); // write(const void*, size_t)
```

Load — either of:
```cpp
// (a) pointer + size, byte-like pointee (uint8_t/char/std::byte)
bool load_state(const uint8_t* data, size_t n);

// (b) any span-ish of byte-like (std::span<const uint8_t>, string_view, ...)
bool load_state(std::span<const uint8_t> bytes);
```

`load_state` returning `false` (or `void` load spellings — also accepted —
which cannot fail) means: reject the blob **without partial application**;
the binding reports failure to the host and the object keeps playing with
its previous/default state. Bindings never crash on malformed blobs; the
processor must uphold the same.

### Container formats

**CLAP** (`clap.state`, new — no legacy to honor):

```
u32 magic 'AVCS'   u32 version = 1
u32 param_count    u32 reserved
param_count × f64  normalized [0,1] parameter values, port order
u64 blob_size      blob_size bytes of custom state (0 if none)
```

Native little-endian (all shipping targets). On load: unknown magic/version
=> reject; `param_count` mismatching the current build => the common prefix
is applied (parameter lists evolve; ordinal prefix matching is what the
VST3 tier-0 behavior already implies).

**VST3** (must stay byte-compatible with existing sessions): the component
stream keeps its exact current layout — N doubles — and the custom blob is
**appended** as a trailer:

```
...existing N doubles...
u32 magic 'AVCS'   u32 version = 1
u64 blob_size      blob_size bytes
```

- Old sessions (no trailer): EOF after the doubles => load params, no blob,
  success. New sessions in old builds: the trailer is simply never read.
- `Controller::setComponentState` reads exactly N doubles and stops — it is
  untouched and unaffected by the trailer.

**vintage**: `effGetChunk/effSetChunk` + `effFlagsProgramChunks`, same
container as CLAP. (Left as follow-up; see Status at the end.)

### Ordering & merge semantics

Custom state **complements** parameter state; parameters are applied first,
then the blob. Rationale: a processor that exposes both macro params and a
blob usually re-derives runtime state from the blob on load; params
land first so `load_state` — which typically ends in a full reconfigure —
wins where they overlap. Processors that persist everything in the blob can
simply ignore the ordering.

For polyphonic effects (`effect_container` with multiple instances), the
blob is saved from the first instance and loaded into every instance.

### Threading

Save/load run on whatever thread the host calls the binding's state entry
points on (CLAP: main thread per spec; VST3: UI thread, host-serialized
against processing in practice). This matches how both bindings already
mutate parameter values from these same calls — no new locking is
introduced. Processors needing handover discipline should stage internally
(load into a shadow, swap at a safe point).

## Rejected alternatives

- **Reflecting non-parameter members automatically** (serialize the whole
  `inputs`/private state via aggregate reflection): attractive but wrong
  layer — silent format instability with every code change, no versioning
  story, and types like allocated buffers can't round-trip naively. Opaque
  blob + processor-owned versioning is strictly more honest.
- **A `state` port type** (à la `file_port`): state is object-level, not a
  dataflow port; forcing it into the port model would create a port that no
  host UI should show.
- **JSON/CBOR structured state** (the dump binding's writers): pushes a
  format choice on processors; can be layered *on top of* the blob by any
  processor that wants it.
- **Requiring exactly one signature**: against house style — every existing
  concept (controls, callbacks, workers) accepts several ergonomic
  spellings and normalizes in the wrapper layer.

## Implementation map

- `include/avnd/concepts/state.hpp` — concepts (`has_save_state`,
  `has_load_state`, `has_custom_state`, spelling variants).
- `include/avnd/wrappers/state.hpp` — `avnd::save_state(obj, sink)` /
  `avnd::load_state(obj, data, n)` normalizing all spellings.
- `include/avnd/binding/clap/audio_effect.hpp` — `CLAP_EXT_STATE`
  implementation (params + blob), advertised from `get_extension`.
- `include/avnd/binding/vst3/component.hpp` — trailer append in
  `getState`, tolerant trailer read in `setState`.
- `tests/test_state.cpp` — static concept-detection tests (all accepted
  spellings + negatives).
- `tests/objects/state_clap.cpp` — Catch2 runtime round-trip through the
  real CLAP binding with in-memory clap streams.

## Status / follow-ups

- CLAP + VST3 implemented; vintage chunks and exposing state in the `dump`
  tool are follow-ups.
- VST3 path compiles against the SDK but has no runtime test here (needs
  `VST3_SDK_ROOT` + an IBStream fake); the CLAP runtime test covers the
  shared wrapper logic.
- The `std::vector<uint8_t> save_state()` / `bool load_state(const uint8_t*,
  size_t)` pair is the expected common spelling and satisfies
  `avnd::has_custom_state` as-is.
