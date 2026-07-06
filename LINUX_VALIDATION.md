# Linux validation & continuation guide (branch `avendish-ui-linear`)

Context document for an agent picking up this branch on a Linux machine.
Everything below was built, tested and validator-swept on Windows
(clang-cl + Ninja); **nothing on this branch has ever executed on Linux**.
Your job: run the same validation round, fix what breaks (the X11 paths
were written blind), and then continue the backlog.

## 1. What this branch is

`avendish-ui-linear` adds custom plug-in UI support to avendish
(C++23, header-only, concept-driven). Architecture plan: `CUSTOM_UI_PLAN.md`
(repo root). Summary:

- **Soft UI runtime** (`include/avnd/binding/ui/soft/`): Nuklear (widgets,
  command queue) rendered through canvas_ity (software vector rasterizer)
  into an RGBA8 framebuffer, windowed via pugl (host-parented child
  windows). Authors write a declarative `struct ui` (halp layout DSL,
  Tier A), custom `paint()` widgets (Tier B), or bring their own
  framework via `T::ui::window` = open/close/idle/size
  (Tier C, `include/avnd/concepts/gui_window.hpp`).
- **Editor glue per binding**: CLAP (`binding/clap/gui.hpp`),
  VST2 (`binding/vintage/gui.hpp`), VST3 (`binding/vst3/view.hpp`),
  standalone preview (`avnd_make_ui_preview`), WASM
  (`binding/ui/soft/wasm.hpp` + the standalone-page pipeline).
- **Message bus**: `ui::bus` / Tier C endpoints over lock-free moodycamel
  queues; VST3 crosses the component/controller split via IMessage with
  a pfr-based serializer (`binding/ui/serialization.hpp`) and a UI-thread
  pump protocol (see the header comment in `binding/vst3/bus.hpp` —
  IMessage may only be allocated/sent on the UI thread).
- **Gestures**: `avnd::gui_host` fn-ptr protocol (flat param index,
  normalized [0,1]), one begin/end per drag.
- Extensive conformance fixes to the clap/vst3/vintage bindings landed on
  this branch after validator sweeps (see `git log`, commits mentioning
  #153/#154): per-call 32/64-bit negotiation, sample-accurate control
  storage lifecycle, null-channel scratch substitution, note dialects,
  per-port channel counts, `avnd::wire_fallback_callbacks`.

## 2. Building (Linux)

```sh
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DFMT_MODULE=OFF \
  -DAVND_ENABLE_CLAP=ON -DAVND_ENABLE_VINTAGE=ON \
  -DAVND_ENABLE_VST3=ON -DAVND_FETCH_VST3_SDK=ON
cmake --build build
```

Notes:
- **`-DFMT_MODULE=OFF` is required**: the fmt version fetched since the
  main rebase auto-enables its C++-modules target when CMake supports it,
  and then hard-requires a module-scanning toolchain. (A pinned
  `FMT_MODULE=OFF` in `cmake/avendish.dependencies.cmake` would be a good
  first commit.)
- Boost headers must be findable (system boost is fine); gcc or clang,
  C++23 required.
- X11 dev packages needed for pugl (libx11, libxrandr, libxext, libxcursor).
- The VST3 SDK is fetched (`AVND_FETCH_VST3_SDK=ON`, pinned
  v3.7.14_build_55).

## 3. The validation round (replicate what was done on Windows)

### 3.1 Test suite

```sh
ctest --test-dir build
```

Windows result: **107/107**. On Linux some UI host tests
(`tests/ui/test_clap_gui.cpp`, `test_vintage_gui.cpp`, `test_vst3_gui.cpp`,
`test_custom_ui_window.cpp`) are Win32-based (HWND creation, WM_* mouse
injection, SetTimer) — check whether they are compiled at all on Linux;
if they are gated out, the X11 equivalents DO NOT EXIST YET and writing
them is part of the work (X11 embed + XTest/XSendEvent input injection).

### 3.2 Plug-in validators

- **clap-validator** (official): download the Linux release of
  https://github.com/free-audio/clap-validator (0.3.2 used on Windows).
  ```sh
  for f in build/clap/*.clap; do
    echo "$f: $(clap-validator validate "$f" 2>&1 | tail -1)"
  done
  ```
  Expected: **84/85 fully pass**. `avnd_test_midi_out` fails 3 tests due
  to a bug in clap-validator itself (`src/plugin/ext/note_ports.rs`
  queries output ports with `is_input=true`; present in 0.3.2 and master;
  an upstream report should be filed if not already).
- **Steinberg VST3 validator**: built from the fetched SDK:
  ```sh
  cmake --build build --target validator
  for f in build/vst3/*.vst3; do
    echo "$f: $(build/bin/validator "$f" | grep 'Result:')"
  done
  ```
  Expected: **zero crashes**; all audio-bearing plug-ins pass everything;
  ~58 parameter-only test objects fail exactly one test
  ("This component does not export any buses") — by design, tracked in
  issue #154 (policy decision pending: synthetic event bus vs not
  building them as VST3).
- **pluginval** (Tracktion, Linux release): strictness 5 then 10 on at
  least `avnd_vst3_ui_test.vst3` — opens the editor, so this exercises
  the X11 embedding path for real.

### 3.3 UI runtime

- Standalone previews: `build/ui_preview/avnd_helpers_ui_ui --frames 60`
  (opens a pugl/X11 window, renders 60 frames, exits 0).
- WASM (if emsdk available): compile
  `tests/ui/wasm_soft_ui_module.cpp` with
  `em++ -std=c++23 -O2 --bind -sMODULARIZE=1 -sEXPORT_ES6=1
  -sENVIRONMENT=node,web -sALLOW_MEMORY_GROWTH=1` plus include paths for
  avendish, nuklear, canvas_ity, magic_enum, fmt, boost, then run
  `node tests/ui/wasm_soft_ui_test.mjs ./wasm_soft_ui.mjs`.
  Passes on Windows (gesture protocol, dirty tracking, external sync).

## 4. Linux-specific code that has NEVER RUN (highest risk, read first)

| Area | File / location | What to verify |
|---|---|---|
| X11 blit | `binding/ui/soft/window.hpp` (XPutImage path) | editor visible, no BadMatch; colors/stride right |
| X11 pugl embedding | same + pugl `puglSetParent` | child window parents into host window (pluginval / a DAW like REAPER) |
| VST3 IRunLoop timer | `binding/vst3/view.hpp` (`start_tick`, Linux branch) | host IRunLoop registration; ticks drive `editor->idle()` + the bus pump; unregister on `removed()` |
| HiDPI | `window_editor::set_scale` under X11 | fractional scales don't leave edge artifacts (the render skips the full clear; only a 2px sliver is cleared) |
| `window_editor::size()` | returns physical px | correct for X11; macOS (logical px) still open, not this machine's problem |

The Steinberg validator does not open editors; **pluginval and a real DAW
(REAPER has a native Linux build with VST3+CLAP) are the editor tests**.

## 5. Continuation backlog (after validation passes)

In priority order (matches the owner's last instructions):
1. ~~Fix whatever the Linux round finds~~ **DONE (2026-07-06)**: two link
   fixes (see `git log`: Linux windowing IIDs in the prototype, missing
   base→pluginterfaces dependency in the fetched SDK); the X11
   blit/embed/IRunLoop code itself ran correctly unmodified.
2. ~~X11-capable UI host tests~~ **DONE**: `tests/ui/gui_test_host.hpp`
   platform layer; all five editor tests run on X11 (107/107 on Linux; the
   VST3 one asserts the IRunLoop register/tick/unregister protocol and a
   fractional setContentScaleFactor mid-session; CustomUiWindow gained a
   raw-Xlib Tier C editor). CI: they need a display — Xvfb works.
3. Perf: dirty-rect readback during drags (`get_image_data`/`putImageData`
   subrects; needs Nuklear command-buffer diffing via
   `NK_ZERO_COMMAND_MEMORY` + memcmp). **Do NOT build the glyph/label
   cache — measured on Linux (2026-07-06) and rejected**, see §6.
4. CLAP port renegotiation for `variable_audio_bus` (currently a safe
   no-op via `avnd::wire_fallback_callbacks` in `wrappers/prepare.hpp`).
5. VST3 param-only objects bus policy (issue #154, left open).
6. wasm: route the ui::bus through the worklet (params/outputs already
   unified; bus messages still go to the canvas's local instance).

## 6. Known facts that will save you time

### Render-perf measurements (Linux, clang 19 -O3, helpers Ui @ 640×480)

Frame = 8.1 ms raster, layout/measure ≈ 0.007 ms (Nuklear caches widths).
Per-command ablation of the raster pass:

- **NK_COMMAND_RECT_FILLED is 5.9 ms of the 8.1** (opaque widget bodies +
  the full-window theme background, all through the vector
  tessellate→AA→float-rgba-composite pipeline). This is the real target.
- Text is only 0.7–0.9 ms (both ProggyClean and DejaVuSans) — the "4 ms"
  estimate from Windows does not reproduce here.
- A label cache (rasterize once per (string,size,color,subpixel phase),
  composite via `canvas.draw_image`) was implemented and benchmarked:
  **8.6 ms vs 8.1 ms baseline — a regression** (draw_image resamples
  through the same float pipeline; for 13 px labels that costs as much as
  tessellating them). Reverted; don't redo it without different numbers.
- `put_image_data` is NOT a fast path for opaque rect fills: it
  re-linearizes per pixel (3 powf/px).
- The plausible big win: an axis-aligned opaque-rect fast path *inside*
  canvas_ity (write premultiplied-linear color spans directly). canvas_ity
  is fetched from upstream a-e-k/canvas_ity though — needs a fork/vendor
  decision by the owner.


- The always-repaint hack is gone: `runtime.tick()` returns a real dirty
  flag; a 16-tick heartbeat bounds staleness for CLAP/VST2 host automation
  (no notification path). Idle frames are ~free; full render ≈13 ms at
  640×480 on a fast desktop core.
- Bus payloads: trivially-copyable fast path, otherwise
  `avnd::bus_serial` (string/vector/aggregate). Payloads a serializer
  can't handle silently disable that bus direction — by design.
- Never add `--embed-file` (or anything FS-touching) to wasm modules:
  the same module runs inside AudioWorkletGlobalScope where emscripten's
  FS init aborts. Fonts ship next to the module and are fetched by the page.
- `Debug` builds of the soft UI are ~6× slower than Release —
  never judge rendering performance on them.
- Two GH issues remain open from this work: #154 (bus-less param-only
  objects) and the three wasm build failures #146/#147/#148
  (variant controls, per-channel-poly, tensor ports — pre-existing,
  unrelated to the UI work).

## 7. Validation status snapshot (Windows, 2026-07-05, this branch)

- Build: clean (clang-cl **with `-DFMT_MODULE=OFF`**).
- ctest: 107/107.
- clap-validator: 84/85 fully pass (see §3.2 for the 85th).
- Steinberg validator: 0 crashes; audio-bearing plug-ins all pass.
- Standalone preview + WASM headless test: pass.
- pluginval strictness 10 on the UI plug-in: pass (Windows).

## 8. Validation status snapshot (Linux, 2026-07-06, this branch)

Environment: Ubuntu 24.04, clang 19, boost 1.90 via
`-DBoost_INCLUDE_DIR=$HOME/ossia/score/3rdparty/libossia/3rdparty/boost_1_90_0`
(system boost is 1.83, too old), build dir `build-linux/`.

- Build: clean after two GNU-ld fixes (Linux windowing IIDs in
  `prototype.cpp.in`; `base`→`pluginterfaces` link dependency for the
  fetched SDK). Both were invisible to MSVC's linker.
- ctest: **107/107** — including the five editor tests, now running
  against real X11 (see §5 item 2).
- clap-validator 0.3.2: **83/84 fully pass**; `avnd_test_midi_out` fails
  its 3 tests due to the validator's own is_input bug, same as Windows.
- Steinberg validator: **0 crashes**; 26 fully pass, 58 param-only test
  objects fail exactly the known "does not export any buses" test (#154).
- pluginval strictness 5 + 10 on `avnd_ui.vst3`, `avnd_helpers_ui.vst3`,
  `avnd_custom_ui_window.vst3`: **all SUCCESS** (editor opened: the X11
  embed + IRunLoop path is exercised for real).
- Standalone previews: render 60 frames on X11 and exit 0; a screen
  capture confirms correct colors (no R/B swap, no stride shear).
- WASM headless test (emsdk from `~/emsdk`): pass.
- Still untested on Linux: a real DAW session (REAPER is installed on
  this machine — worth a manual session when convenient).
