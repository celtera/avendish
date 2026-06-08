# Avendish → Web Audio Modules 2 (WAM2) packaging

This directory contains the templates that wrap an Avendish-generated Emscripten
module (`<c_name>.mjs` + `<c_name>.wasm`, exposing the plug-in via Embind) as a
standards-compliant **Web Audio Modules 2** plug-in, loadable by web DAWs such as
[WAM-Studio](https://github.com/Brotherta/wam-studio).

## Produced package layout

At build time CMake `configure_file`s these templates into
`${CMAKE_BINARY_DIR}/wasm/wam/<c_name>/`:

```
<c_name>/
  index.js            ← WebAudioModule subclass (default export, package entry)
  node.js             ← WamNode (main thread): parameters, events, state
  processor.js        ← WamProcessor (audio thread): the DSP
  gui.js              ← createElement(plugin) → vanilla auto-UI
  avnd-wam-params.js  ← Avendish→WAM parameter mapping (shared, framework-free)
  descriptor.json     ← WAM2 descriptor (name/vendor/apiVersion/…)
  package.json        ← ES-module package manifest + WAM deps
  <c_name>.mjs        ← (copied) the Emscripten ES module factory
  <c_name>.wasm       ← (copied) the DSP binary
```

## Loading it in a WAM host

A WAM host loads the package by dynamically importing `index.js`:

```js
import WAM from 'https://your-host/wasm/wam/<c_name>/index.js';

const groupId = /* the host's WAM group id */;
const instance = await WAM.createInstance(groupId, audioContext);

instance.audioNode.connect(audioContext.destination);
sourceNode.connect(instance.audioNode);

const gui = await instance.createGui();
document.body.appendChild(gui);
```

`createInstance` runs `initialize()` (which loads `descriptor.json`) then
`createAudioNode()`.

## WamEnv / WamGroup bootstrap requirement

WAM2 requires the **audio-thread environment** to be set up before any
`WamProcessor` is registered:

1. The host (or `@webaudiomodules/sdk`'s `initializeWamHost`) installs
   `WamEnv` + a `WamGroup` into the `AudioWorkletGlobalScope`, exposing
   `globalThis.webAudioModules.getModuleScope(moduleId)`.
2. Our `index.js` then calls the SDK's `addFunctionModule(audioWorklet,
   getProcessor, '<c_name>')`, which serializes `processor.js`'s default export
   and `addModule()`s it. Inside the worklet, that function pulls `WamProcessor`
   out of `getModuleScope('<c_name>')` and `registerProcessor`s our subclass.

If you embed this WAM directly (no host SDK), you must perform step 1 yourself —
see `@webaudiomodules/sdk`'s `initializeWamHost(audioContext, [groupId, groupKey])`.

### How the DSP wasm is loaded on the audio thread

`AudioWorkletGlobalScope` has **no `fetch`** and our processor module is
stringified by `addFunctionModule` (so it cannot `import` anything). Therefore:

1. On the **main thread**, `index.js` fetches both the Emscripten ES-module
   *source text* (`<c_name>.mjs`) and the `<c_name>.wasm` bytes.
2. It `postMessage`s them to the processor as
   `{type:'avnd/load', moduleSource, wasmBinary}` (wasm transferred).
3. The processor evaluates `moduleSource` (via a `data:` URL dynamic import, with
   a `Function`-eval fallback) to recover the Emscripten factory, instantiates it
   with `{wasmBinary}`, constructs the Embind plug-in and calls
   `prepare(in, out, 128, sampleRate)`.

## COOP / COEP (cross-origin isolation) requirement

The Emscripten module is built with `-pthread`/atomics and shared-memory, and
some hosts use `SharedArrayBuffer` for their WAM event rings. The page that
loads the WAM **must be cross-origin isolated**, i.e. served with:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

(and every sub-resource served with `Cross-Origin-Resource-Policy: cross-origin`
or same-origin). Without these, `crossOriginIsolated` is `false` and threaded
wasm / `SharedArrayBuffer` will not be available. The DSP audio path itself is
single-threaded (it runs inside the AudioWorklet), so a plain effect can work
without COOP/COEP, but hosts generally require isolation — serve it isolated.

## Parameter mapping

See `avnd-wam-params.js`. Summary:

| Avendish `type`            | WAM `WamParameterInfo.type` | notes |
|----------------------------|-----------------------------|-------|
| `float`                    | `float`                     | `discreteStep` = Avendish `step` or 0 (continuous) |
| `int`                      | `int`                       | `discreteStep` = `step` or 1 |
| `bool` (or widget `toggle`)| `boolean`                   | min 0, max 1, step 1 |
| `enum` / has `values[]`    | `choice`                    | `choices` = the value labels; min 0 .. max N-1 |
| `rgb` / `rgba`             | `float` × 3 / × 4           | one normalized [0;1] param per component, id `<id>/r…a` |
| `xy` / `xyz` / `xyzw`      | `float` × 2 / 3 / 4         | one param per component, id `<id>/x…w`, declared range |
| `string`,`list`,`map`,…    | *(skipped)*                 | no scalar automation; reachable via custom GUI/state |

`minValue`/`maxValue`/`defaultValue` come from the Avendish `min`/`max`/`init`;
`units` from Avendish `unit`; `exponent` is always `1` (linear).

## Testing without a browser

`tests/run.sh [modules-dir]`:

* runs `tests/test-wam-params.mjs` — asserts the mapping + `WamNode` logic in
  Node (SDK base classes stubbed by `tests/sdk-shim.mjs`), against the real
  built modules in `modules-dir` (default `/tmp/avnd-wasm-test`);
* `node --check`s every produced `.js` (placeholders substituted);
* validates `descriptor.json` / `package.json` parse.

A full end-to-end load still requires a real browser + WAM host (AudioWorklet,
`fetch`, `customElements`, a host-provided `WamEnv`/`WamGroup`).
```
