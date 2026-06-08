<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Avendish WebAssembly — JavaScript runtime (Phase 2)

This directory contains the JavaScript runtime that turns a compiled Avendish
WASM plug-in module (`<c_name>.mjs` + `<c_name>.wasm`, an Emscripten
`MODULARIZE=ES6` build that default-exports a module factory and exposes an
Embind class named exactly `<c_name>`) into:

- **(A)** a browser-standalone audio page using an `AudioWorklet`, and
- **(B)** a headless offline runner for Node / Deno / Bun.

The C++/Embind module contract and the shared ring-buffer byte layout are fixed
by the backend (`include/avnd/binding/wasm/{module,ringbuffer}.hpp`) and are
**not** changed here — the JS is written to match them byte-for-byte.

## Files

| File | Kind | Purpose |
|------|------|---------|
| `avnd-ringbuf.js` | runtime | SPSC wait-free `RingBuffer` over a `SharedArrayBuffer`, byte-compatible with the C++ `wasm::ring_buffer`. |
| `avnd-audio-helper.js` | runtime | `HeapAudioBuffer`: planar float32 storage inside the WASM heap, with AudioWorklet copy-in/out and detached-buffer-safe views. |
| `avnd-dsp.js` | runtime | `AvndProcessor`: AudioContext-free DSP marshaling (heap copy-in → `process()` → copy-out). The unit-testable core shared by the worklet and the node runner. |
| `avnd-ui.js` | runtime | Auto-UI: the widget↔input mapping (`makeControl`) plus the structured layout renderer (`buildLayoutUI` / `buildAutoUI`) that walks `module.getUILayout()` (groups/tabs/hbox/vbox/split/grid + control items) and renders custom `paint()` items onto a `<canvas>` via `module.paintCustom(itemIndex, ctx2d)` in a requestAnimationFrame loop. Falls back to a flat parameter list when the plug-in has no `ui`. |
| `worklet.js.in` | template | The `AudioWorkletProcessor`. Thin shell over `AvndProcessor`. |
| `standalone.html.in` | template | Self-contained host page + auto-UI. |
| `standalone.js.in` | template | Page controller: boots the AudioContext, loads the worklet, feeds it the wasm, builds the parameter UI. |
| `avnd-node-runner.mjs` | runtime + CLI | Headless offline renderer (`render()` / `Runner`) plus a CLI. |
| `avnd-dev-server.mjs` | tool | Static file server that sets the required COOP/COEP headers. |
| `tests/` | tests | `node` test scripts for the ring buffer, the heap buffer, and the DSP / node-runner path. |

### `@…@` substitutions

The `*.in` templates use two placeholders, substituted at install/configure
time (CMake `configure_file`, or by hand when copying):

- `@AVND_WASM_C_NAME@` — the Embind class / module base name, e.g. `avnd_lowpass`.
- `@AVND_WASM_NAME@` — the human-readable display name, e.g. `Lowpass`.

After substitution the files are renamed to, respectively,
`<c_name>-worklet.js`, `index.html` (or `<c_name>-standalone.html`) and
`<c_name>-standalone.js`, alongside `<c_name>.mjs` / `<c_name>.wasm` and the
non-templated `.js`/`.mjs` runtime files.

## JS API

### `RingBuffer` (`avnd-ringbuf.js`)

SPSC wait-free ring over a flat byte region. Layout (identical to the C++ side
and to padenot/ringbuf.js):

```
[0..3]   read index  (uint32, free-running)
[4..7]   write index (uint32, free-running)
[8.. ]   capacity bytes of storage,  capacity = total - 8,  MUST be power of two
```

```js
import { RingBuffer } from './avnd-ringbuf.js';

const total = RingBuffer.storageSize(1024);     // 1024 + 8-byte header
const rb = new RingBuffer(new SharedArrayBuffer(total));

rb.push(uint8);          // -> bytes actually written (clamped to free space)
rb.pop(uint8);           // -> bytes actually read
rb.availableRead();      // writeIdx - readIdx
rb.availableWrite();     // capacity - availableRead()
rb.clear();              // consumer-side reset
rb.pushFloats(f32);      // all-or-nothing; -> floats pushed (0 or f32.length)
rb.popFloats(f32);       // -> floats read
```

Synchronisation discipline (standard wait-free SPSC): the producer does
`Atomics.load(readIdx)` (acquire) before writing data, then
`Atomics.store(writeIdx)` (release); the consumer does `Atomics.load(writeIdx)`
(acquire) before reading, then `Atomics.store(readIdx)` (release). This is
byte/semantics compatible with the C++ `wasm::ring_buffer`, so a future Worker
producer (C++ or JS) interoperates without a copy.

### `HeapAudioBuffer` (`avnd-audio-helper.js`)

```js
import { HeapAudioBuffer } from './avnd-audio-helper.js';

const buf = new HeapAudioBuffer(M, frames, channels); // _malloc planar float32
buf.ptr;                       // byte offset to pass to plugin.process(...)
buf.getChannelData(c);         // fresh Float32Array view over channel c (len=frames)
buf.fromInterleaved(f32, srcCh);
buf.toInterleaved(out?);
buf.copyFromAudioWorklet(inputs[0]);   // planar Float32Array[ch] -> heap
buf.copyToAudioWorklet(outputs[0]);    // heap -> planar Float32Array[ch]
buf.free();
```

Planar layout matches the contract: channel `c` starts at byte offset
`ptr + c*frames*4` (float index `ptr/4 + c*frames`). Channel views are **never
cached** across calls: with `ALLOW_MEMORY_GROWTH`, `M.HEAPF32.buffer` can be
replaced (the old `ArrayBuffer` detached) on any `_malloc` that grows linear
memory, so every accessor re-derives its view from the current `M.HEAPF32`.
Channel-count mismatches are handled: surplus heap channels are zero-filled,
surplus worklet channels are dropped.

### `AvndProcessor` (`avnd-dsp.js`)

The AudioContext-free DSP core. Constructs the plug-in, `prepare()`s it,
allocates heap buffers sized to the **negotiated** channel counts, and exposes
`process(inputPlanes, outputPlanes)` plus parameter/output passthrough. Both the
worklet and the node runner are thin wrappers over this, which is why the hot
path is unit-testable in plain Node.

### `render()` / `Runner` (`avnd-node-runner.mjs`)

```js
import { render } from './avnd-node-runner.mjs';

const r = await render('./avnd_lowpass.mjs', {
  cName: 'avnd_lowpass', in: 2, out: 2, rate: 48000, frames: 256,
});

r.inChannels; r.outChannels; r.frames; r.sampleRate; r.name;

const outs = r.processBlock([inCh0, inCh1]);     // Float32Array[] (len = outChannels)
const big  = r.renderToBuffer(inputPlanes);      // chunked render of an arbitrary-length signal

r.setParameter(0, 0.5); r.getParameter(0);
r.setParameterNormalized(0, 0.5); r.getParameterNormalized(0);
r.getOutputCount(); r.getOutputInfo(i); r.getOutputValue(i);
r.destroy();
```

CLI (renders a unit impulse and prints per-channel stats):

```sh
node avnd-node-runner.mjs <module.mjs> --cname NAME --in N --out N --rate 48000 --frames 256
```

## How the worklet loads the wasm

There is **no `fetch()` in `AudioWorkletGlobalScope`**, so Emscripten cannot
download the `.wasm` itself from inside the worklet. Instead:

1. The page calls `audioWorklet.addModule('<c_name>-worklet.js')`. Because
   `addModule` loads the file as an **ES module**, the worklet's *static*
   `import ModuleFactory from './<c_name>.mjs'` resolves inside the worklet
   scope. (The factory itself does not fetch anything when given the bytes.)
2. The **main thread** (which *does* have `fetch`) fetches the `.wasm` bytes and
   `postMessage`s them to the processor as
   `{ type:'init', wasmBinary, sampleRate, requestedIn, requestedOut }`
   (the `ArrayBuffer` is transferred).
3. The processor instantiates the module from those bytes via the factory's
   `wasmBinary` option — no fetch, and we `await` the factory promise exactly
   once before audio flows. It then constructs the plug-in and
   `prepare(in, out, 128, sampleRate)`.

The render quantum is locked to **128 frames** (the AudioWorklet quantum) and
`prepare()` is called with 128, so no block-size adaptation is needed in Phase
2. The DSP marshaling is factored into `AvndProcessor` so a future block-size
adapter can batch quanta without touching the audio-thread shell. Parameter
messages (`{type:'setParam'|'setParamNorm', index, value}`) that arrive before
`init` completes are queued and drained once the plug-in is ready; control
outputs are posted back as `{type:'outputs', values}` throttled to ~20 Hz.

## COOP / COEP requirement

`SharedArrayBuffer` (and therefore threaded WASM and the shared ring buffer) is
only available when the page is **cross-origin isolated**. That requires the
document to be served with:

```
Cross-Origin-Opener-Policy:   same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Without them, `self.crossOriginIsolated` is `false` and
`new SharedArrayBuffer(...)` throws / is unavailable. The single-threaded audio
path (DSP running synchronously inside the worklet) still works without
isolation, so the standalone page shows a warning banner instead of failing —
but the future Worker producer that drives the ring buffer does need isolation.

`avnd-dev-server.mjs` sets these headers (plus
`Cross-Origin-Resource-Policy: same-origin` and the correct
`application/wasm` MIME type) for local development:

```sh
node avnd-dev-server.mjs /tmp/avnd-p2 --port 8080
# -> http://127.0.0.1:8080/
```

## Tests

```sh
# RingBuffer needs no module:
node tests/test-ringbuf.mjs

# These need a built <c_name>.mjs + .wasm (see tests/build-wasm-example.sh):
node tests/test-audio-helper.mjs  /tmp/avnd-p2/avnd_lowpass.mjs
node tests/test-node-runner.mjs   /tmp/avnd-p2/avnd_lowpass.mjs

# Or all at once:
tests/run.sh /tmp/avnd-p2/avnd_lowpass.mjs
```

To build the Lowpass example module first:

```sh
source ~/emsdk/emsdk_env.sh
include/avnd/binding/wasm/tests/build-wasm-example.sh \
    examples/Raw/Lowpass.hpp examples::Lowpass avnd_lowpass /tmp/avnd-p2
# then copy the js/ files into /tmp/avnd-p2, substituting
#   @AVND_WASM_C_NAME@ = avnd_lowpass,  @AVND_WASM_NAME@ = Lowpass
```

## What still needs a real browser

The DSP path (heap copy-in → `process()` → copy-out), the wasm-binary
instantiation flow, parameter set/get, the ring buffer, and the node runner are
all exercised headlessly in Node. The following can only be fully validated in a
real browser with a live `AudioContext`, and are therefore **not** covered by
the Node tests:

- `audioWorklet.addModule()` actually loading the ES module in
  `AudioWorkletGlobalScope` and the static `import` resolving there.
- The `AudioWorkletNode` lifecycle, real 128-frame `process()` callbacks driven
  by the audio render thread, and channel negotiation against
  `outputChannelCount`.
- `getUserMedia` (mic), `decodeAudioData` (file), and the oscillator source.
- That a page served with the COOP/COEP headers reports
  `crossOriginIsolated === true` and can construct a `SharedArrayBuffer`.
