# Custom UI support for Avendish plug-ins

Plan for letting Avendish plug-in authors ship graphical user interfaces that
work in plug-in hosts (VST2, VST3, CLAP editors), standalones, WebAssembly and
even bare-metal targets (ESP32-class LCDs) — without depending on Qt or a
browser, and without changing the author-facing model that already exists.

Status: proposal. Date: 2026-07-02.

---

## 1. Goals and constraints

1. **Author API stability.** Avendish already defines a complete, backend-neutral
   UI authoring model (`struct ui` declarative layouts, `avnd::painter` custom
   widgets, UI↔processor message bus). A plug-in written today against that
   model must gain a working VST/CLAP editor *without source changes*.
2. **Framework neutrality.** Authors who want to bring their own UI stack
   (Dear ImGui, JUCE, Qt, raw GDI…) must be able to, through a small set of
   host-attachment concepts. The reference implementation must itself be built
   on those concepts, proving they are sufficient.
3. **Extreme portability.** The reference stack must build with MSVC, clang on
   msys2, gcc on FreeBSD, Emscripten, and xtensa/riscv gcc for ESP32. This
   forces: no mandatory GPU context, no mandatory OS windowing, C/C++ only,
   no runtime dependencies beyond libc (and even that optional on MCU).
4. **The VST editor contract.** Hosts hand the plug-in a *native parent window*
   (HWND / NSView* / X11 Window) and a UI-thread idle/timer callback. The
   plug-in renders a child into it and never owns the event loop. Every layer
   below must fit this inversion of control.
5. **Permissive licensing** for everything linked into a binary plug-in.

## 2. What exists today (inventory)

The author-facing model is done; the host-facing side is missing.

| Piece | State | Where |
|---|---|---|
| Declarative layout DSL (hbox/vbox/grid/tabs/split/custom) | ✅ complete | `include/halp/layout.hpp`, concepts in `include/avnd/concepts/layout.hpp` |
| Canvas painter concept | ✅ complete | `include/avnd/concepts/painter.hpp` (paths, béziers, transforms, text, images) |
| Custom interactive widgets (`paint()` + `mouse_press/move/release`) | ✅ complete | e.g. `examples/Advanced/UI/Multislider.hpp` |
| Automation-gesture transactions | ✅ complete | `halp::transaction` in `include/halp/custom_widgets.hpp` |
| UI↔processor message bus (auto-serializing) | ✅ complete | `include/avnd/concepts/ui.hpp`, `examples/Helpers/UiBus.hpp` |
| Painter implementations | Qt (score), Canvas2D (`binding/wasm/painter.hpp`), QML | host-bound: need Qt or a browser |
| Nuklear prototype | ⚠️ dead end for plug-ins | `binding/ui/nuklear_layout_ui.hpp` owns a GLFW window + desktop GL 4.5; cannot embed into a host parent, cannot run on MCU |
| VST2 editor | ❌ | `EditOpen/EditClose/EditIdle/EditGetRect` opcodes declared in `vintage.hpp:91-97` but unhandled in `dispatch.hpp` |
| VST3 editor | ❌ | `controller_base.hpp:102` — `createView()` returns `nullptr` |
| CLAP GUI | ❌ | no `clap.gui` extension in `binding/clap/audio_effect.hpp` |
| Parameter mirror for UIs | ✅ (VST2) | `binding/vintage/atomic_controls.hpp` atomic float array + `parameter_input_introspection` |

## 3. Library survey → picks

Extensive survey (July 2026) of microui, Nuklear, Dear ImGui, raygui, GuiLite,
Jet, LVGL, Clay, Elements, NanoGUI/NanoVG, RmlUi, Slint, NAppGUI, U8g2, luigi,
TIGR, Fenster, olive.c, egui, plus plugin prior art (pugl, DPF, iPlug2, CPLUG,
VSTGUI, JUCE, the free-audio clap-imgui examples). Full notes in §11.

Key structural finding: the plug-in UI field has converged on
**windowing shim + renderer-agnostic widget layer whose output you rasterize
yourself** (DPF, CPLUG, clap-imgui all do this). Libraries that *own their
window* (raygui/raylib, TIGR, NanoGUI, sokol) structurally fail the VST
contract; libraries that *emit commands or pixels* (Nuklear, microui, Clay,
ImGui, LVGL) pass it.

Picks, per layer (each layer independently swappable):

| Layer | Pick | Why | Alternatives kept open |
|---|---|---|---|
| Window embedding (desktop) | **pugl** (ISC, C99, zero deps) | `puglSetParent()` before realize = exactly the VST contract; `puglUpdate(world, 0)` from host idle; stub backend for pure software blitting; de-facto standard in LV2/DPF/CPLUG world | ~600-line in-tree shim if we ever want zero fetch |
| Standard widgets + input logic | **Nuklear** (MIT/Unlicense, single C89 header, ~18 kLoC) | Only surveyed library with: in-tree software rasterizer (`rawfb`), native `nk_knob`, fixed-buffer no-libc mode (MCU-proven), active maintenance (v4.13.3, 2026); text editing/combos/skinning for free | Dear ImGui (richest ecosystem, GPU-oriented, PSRAM-hungry on MCU); LVGL (if the embedded-product end dominates); microui (pedagogical minimum, dormant) |
| `avnd::painter` rasterizer | **canvas_ity** (ISC, single header ~2.3 kLoC) | HTML5-Canvas semantics map 1:1 onto `avnd::painter` (which was modeled on Qt/Canvas2D); pure software, no deps, TTF text, gradients, clipping | PlutoVG (C, MIT, lighter surfaces — better for tiny-RAM MCU); ThorVG |

Licensing: ISC + MIT/Unlicense + ISC are all compatible with Avendish's
dual-licensed concept headers and with proprietary plug-in binaries.

Why not "just Nuklear" for custom widgets too: Nuklear's command list has no
arbitrary path fill, no bézier fill, no transforms — it cannot implement
`avnd::painter`. Why not "just canvas_ity" for everything: then we own text
editing, focus, IME, combo popups… That is exactly the code one should not
write twice. Hybrid: Nuklear draws/handles *standard* controls, canvas_ity
backs *custom* `paint()` widgets, both into the **same RGBA framebuffer**.

## 4. Architecture overview

Three layers, top-down. Only Layer 1 is visible to plug-in authors.

```
┌────────────────────────────────────────────────────────────────────┐
│ L1  Author API (unchanged + one new escape hatch)                  │
│     A. struct ui { hbox/vbox/… }         — declarative, zero code  │
│     B. custom_control<W,&ins::x> + paint(avnd::painter auto)       │
│     C. NEW struct ui::window { open/idle/close }  — any framework  │
├────────────────────────────────────────────────────────────────────┤
│ L2  Reference UI runtime  (avnd/binding/ui/soft/)                  │
│     "pixels in, events in, pixels out" — no OS calls in the core   │
│     • layout walk of struct ui   → Nuklear widgets (nk_knob, …)    │
│     • custom paint() widgets     → soft_painter (canvas_ity)       │
│     • both composite into one RGBA8 framebuffer (nk rawfb)         │
│     • input: pointer/key events pushed in by the shell             │
│     • param edits → gesture hooks; bus msgs → SPSC queues          │
├────────────────────────────────────────────────────────────────────┤
│ L3  Shells (own the window/surface, drive the runtime)             │
│     desktop plug-in: pugl child window, blit framebuffer           │
│         VST2  EditOpen/EditIdle/EditClose/EditGetRect              │
│         VST3  IPlugView (attached/removed/onSize, IRunLoop timer)  │
│         CLAP  clap.gui + clap.timer-support                        │
│     standalone: pugl top-level window                              │
│     wasm: framebuffer → canvas putImageData (or existing           │
│           Canvas2D painter path, which stays)                      │
│     ESP32/bare: no shell — user calls render()/pointer_event()     │
│           and flushes the framebuffer to the LCD                   │
└────────────────────────────────────────────────────────────────────┘
```

The reference runtime (L2) is deliberately implemented *as* a Tier C provider:
the plug-in bindings only ever see the Layer-1C concepts. That keeps the
binding glue framework-agnostic and proves the escape hatch is complete.

## 5. Layer 1C — host-attachment concepts (new)

New header `include/avnd/concepts/gui_window.hpp` (names bikesheddable):

```cpp
namespace avnd
{
enum class gui_api : uint8_t
{ win32_hwnd, cocoa_nsview, x11_window, wayland_surface, web_canvas, framebuffer };

// What the host/binding gives the UI when opening it.
struct gui_parent
{
  void*   handle;   // HWND / NSView* / (X11 Window)(uintptr_t) / css selector
  gui_api api;
  double  scale;    // host-reported HiDPI factor, 1.0 if unknown
};

// Callbacks the UI can invoke on the binding. All must be called on the UI thread.
struct gui_host
{
  // parameter automation gestures, param index = flat index in
  // parameter_input_introspection order, value normalized to [0,1]
  void (*begin_edit)(void* ctx, int param);
  void (*perform_edit)(void* ctx, int param, double normalized);
  void (*end_edit)(void* ctx, int param);
  void (*request_resize)(void* ctx, int w, int h); // may be refused by host
  void* ctx;
};

// The escape hatch: a plug-in (or the reference runtime on its behalf)
// exposes T::ui::window with this shape.
template <typename W>
concept gui_windowed_ui = requires(W w, gui_parent p, gui_host h)
{
  w.open(p, h);              // create child window/context inside p, show it
  w.close();                 // tear down (host may destroy parent right after)
  w.idle();                  // host UI-thread tick: pump events, repaint if dirty
  { w.size() } -> std::same_as<std::pair<int,int>>; // current logical size
};

// optional refinements, detected with requires():
//   w.set_scale(double)          — host DPI changed
//   w.set_size(int, int)         — host resized us (resizable editors)
//   { W::resizable() } -> bool
//   { w.supports(gui_api) } -> bool   — default: platform default only
}
```

Design notes:

- `idle()` is the only clock. Every host in scope provides one (VST2
  `effEditIdle` + a 30 Hz timer we install ourselves where hosts are lazy,
  VST3 `IRunLoop`/platform timer, CLAP `timer-support`, `requestAnimationFrame`
  on the web, the user's main loop on MCU). The runtime never spawns threads.
- Parameter *values* do not travel through `gui_host`; they go through each
  binding's existing parameter mirror (see §8). `gui_host` only carries the
  gesture protocol, which is the one thing the author model (`halp::transaction`)
  already distinguishes and which every host API demands
  (`audioMasterAutomate`, `IComponentHandler::begin/perform/endEdit`,
  CLAP `GESTURE_BEGIN/PARAM_VALUE/GESTURE_END` events).
- The bus (`ui::bus::send_message` / `process_message`) transport is owned by
  the binding (§8), not by the window concept — same split as today in score.

Author-facing Tier C example (bring-your-own-framework):

```cpp
struct MyPlugin {
  ...
  struct ui {
    struct window {
      static constexpr auto width = 640, height = 400;
      void open(avnd::gui_parent p, avnd::gui_host h);  // e.g. create a JUCE
      void close();                                     // component, an ImGui
      void idle();                                      // context, a Qt widget…
      std::pair<int,int> size() const;
    };
  };
};
```

If `T::ui` has no `window` member but is a layout tree (today's model), the
binding instantiates the reference runtime's `soft::window<T>` which satisfies
`gui_windowed_ui` — Tier A/B authors write nothing.

## 6. Layer 2 — reference runtime `avnd/binding/ui/soft/`

Proposed files:

```
include/avnd/binding/ui/soft/
  framebuffer.hpp     // rgba8 span + width/height/stride, no ownership
  painter.hpp         // soft_painter: avnd::painter over canvas_ity
  widgets.hpp         // layout-DSL → Nuklear walk (ports nuklear_layout_ui.hpp)
  runtime.hpp         // soft::ui_runtime<T>: composition + input + dirty tracking
  window.hpp          // soft::window<T>: gui_windowed_ui via pugl (desktop)
  surface.hpp         // soft::surface<T>: windowless variant (wasm/MCU)
```

### 6.1 The pure core: `soft::ui_runtime<T>`

No OS includes. Owns:

- one `nk_context` in `nk_init_fixed` mode (fixed arena, MCU-friendly),
- one canvas_ity canvas sized to the root layout,
- the instantiated `typename avnd::ui_type<T>::type` layout tree,
- pointers to the parameter mirror + bus queues supplied by the binding.

API (called by shells):

```cpp
struct pointer_event { double x, y; int buttons; double wheel; };
struct key_event     { uint32_t keycode; char32_t ch; bool down; };

template <typename T>
struct ui_runtime
{
  void set_viewport(int w, int h, double scale);
  void pointer(pointer_event);
  void key(key_event);
  bool tick();                       // widget pass; true if repaint needed
  void render(framebuffer out);      // nk rawfb pass + custom-widget canvases
};
```

`tick()` runs the Nuklear frame: walks the layout tree exactly like the
existing `nkl::layout_ui` (`binding/ui/nuklear_layout_ui.hpp:74-181` is 80 %
reusable), mapping widget hints of halp controls to Nuklear widgets:

| halp widget hint | Nuklear |
|---|---|
| `knob` | `nk_knob_float/int` |
| `hslider`/`vslider` | `nk_slider_float/int` |
| `spinbox` | `nk_property_float/int` |
| `toggle`/`checkbox` | `nk_checkbox_label` |
| `combobox`/enums | `nk_combo` |
| `button`/`bang` | `nk_button_label` |
| `lineedit` | `nk_edit_string` |
| `xy` pad, others | custom-painted fallback (Tier B machinery) |

For **Tier B custom widgets** (`custom_control<W,&ins::x>` etc.), the walk
reserves the widget's rect via `nk_widget()`, routes mouse events to
`W::mouse_press/move/release` (mapped into widget-local coordinates), and at
render time runs `W::paint(soft_painter{...})` with the painter translated
onto the widget's rect in the shared framebuffer. `ctx.update()` marks the
widget dirty (matching the Qt/WASM painter contract). Static-sized widgets
cache their canvas; only dirty ones re-rasterize.

`render()` replays the Nuklear command queue through the `rawfb` software
rasterizer into the caller's framebuffer, then composites the custom-widget
canvases. One font atlas (stb_truetype via Nuklear) shared with canvas_ity's
text where possible; scale changes rebake the atlas (the known Nuklear
HiDPI cost — acceptable, it happens on monitor changes only).

`halp::transaction` on custom widgets and Nuklear's
`nk_widget_has_mouse_click_down` edges on standard widgets both funnel into
`gui_host::begin/perform/end_edit`.

### 6.2 Desktop shell: `soft::window<T>` (pugl)

- `open()`: `puglSetParent(view, (PuglNativeView)p.handle)` +
  `puglSetBackend(view, puglStubBackend())`, realize, show.
- `idle()`: `puglUpdate(world, 0)`; on expose, `runtime.render()` into a
  per-view pixel buffer and blit — Win32 `StretchDIBits`, X11
  `XPutImage`/XShm, macOS drawing the buffer into the NSView's layer
  (pugl stub backend exposes the native surface for all three).
- Wayland: not a plug-in concern in practice (hosts hand out X11 windows);
  standalone Wayland goes through pugl's normal path when it lands there.
- No global state: pugl worlds are per-instance, so many plug-in instances in
  one process coexist (mandatory for plug-ins; rules out anything with
  singletons).

### 6.3 Windowless shell: `soft::surface<T>` (WASM, ESP32, tests)

```cpp
soft::surface<MyPlugin> ui{eff};
ui.set_viewport(320, 240, 1.0);
// main loop / rAF / lvgl-style tick:
ui.pointer({touch_x, touch_y, touch_down});
if (ui.tick())
{
  ui.render({psram_buf, 320, 240});
  esp_lcd_panel_draw_bitmap(panel, 0, 0, 320, 240, psram_buf);
}
```

On WASM this same path blits via `putImageData`; the existing Canvas2D painter
backend (`binding/wasm/painter.hpp` + `ui_bridge.hpp`) remains as the
DOM-native alternative. On MCU, memory is the constraint: Nuklear fixed arena
(~64–256 KB), RGBA8 framebuffer (320×240 = 300 KB → PSRAM, or render in
horizontal bands like LVGL to stay in SRAM — canvas_ity can't band easily, so
the MCU tier may prefer the PlutoVG painter variant; the painter is behind the
concept, so this is a swap, not a redesign). Golden-test hook for free: the
differential harness can `render()` to a buffer and image-diff UIs headlessly.

## 7. Layer 3 — plug-in binding glue

### 7.1 VST2 (`binding/vintage`)

`dispatch.hpp` gains (only when `avnd::gui_windowed_ui<...>` or a layout `ui`
exists — SFINAE'd so UI-less plug-ins compile exactly as today):

```cpp
case EffectOpcodes::EditGetRect: {
  static vintage::Rect r{0, 0, (int16_t)h, (int16_t)w};
  *(vintage::Rect**)ptr = &r;                       return 1; }
case EffectOpcodes::EditOpen:
  self.editor.open({.handle = ptr, .api = native_api(), .scale = 1.0},
                   self.editor_host());             return 1;
case EffectOpcodes::EditClose:  self.editor.close(); return 1;
case EffectOpcodes::EditIdle:   self.editor.idle();  return 1;
```

plus `EffectFlags::HasEditor` in `processor_setup.hpp`, and
`gui_host::perform_edit` → `setParameterAutomated`/`audioMasterAutomate`
(gestures via `audioMasterBeginEdit/EndEdit`). Parameter values reuse
`atomic_controls.hpp` verbatim — the editor reads/writes the same atomics the
host automation writes.

### 7.2 VST3 (`binding/vst3`)

New `binding/vst3/view.hpp`: `template <typename T> struct plug_view final :
Steinberg::IPlugView` — `isPlatformTypeSupported` (kPlatformTypeHWND /
NSView / X11EmbedWindowID), `attached()` → `editor.open(...)`, `removed()` →
`close()`, `onSize`, `canResize`. Ticks: Win32 `SetTimer`, macOS
`CFRunLoopTimer`, Linux the host's `IRunLoop` (query from the frame — this is
the well-known VST3-Linux dance and the reason `idle()` is our only clock).
`controller_base.hpp:102` `createView("editor")` returns it. Gestures map to
`IComponentHandler::beginEdit/performEdit/endEdit` with VST3-normalized
values — the machinery for param normalization already exists in the
controller.

### 7.3 CLAP (`binding/clap`)

Implement `clap_plugin_gui` (`is_api_supported`, `get_preferred_api`,
`create`, `destroy`, `set_scale` → `set_scale()`, `get_size`, `can_resize`,
`set_size`, `set_parent` → `open()`, `show`, `hide`) plus
`clap_plugin_timer_support`/`on_timer` → `idle()` (fallback: hosts that lack
timer support get a pugl-side timer). Gestures become
`CLAP_EVENT_PARAM_GESTURE_BEGIN/PARAM_VALUE/GESTURE_END` pushed on the
out-event queue via the existing param flush path. CLAP is the cleanest of the
three and should be the first target.

### 7.4 Standalone / examples

`avnd_make_standalone_ui(...)`: pugl top-level window + `soft::window<T>`,
replacing the GLFW/GL4.5 demo path, so every example with a `ui` gets a
runnable desktop preview on Windows/macOS/X11/FreeBSD with zero GPU
requirements.

## 8. Cross-cutting: parameters, bus, threading

- **Threading model:** UI code runs only on the host UI thread (inside
  `open/idle/close`). Audio runs on the audio thread. They meet only at:
  1. the **parameter mirror** — per-binding, already exists or is trivial:
     vintage's `std::atomic<float>` array; CLAP/VST3 event queues with a
     UI-side shadow value refreshed in `idle()`;
  2. the **message bus** — two SPSC queues (moodycamel `concurrentqueue` is
     already a dependency) carrying the serialized messages;
     UI→proc drained at the top of `process()`, proc→UI drained in `idle()`
     which then calls `ui::bus::process_message(ui, msg)`. This reuses the
     exact serialization machinery score's bus uses today; only the transport
     is new (and shared by all three plug-in bindings).
- **Value display / control feedback:** `idle()` diffs the mirror against the
  last-shown values and marks affected widgets dirty; no repaint when nothing
  changed (important for laptop battery + MCU).
- **HiDPI:** logical-pixel layout; the framebuffer is `ceil(w*scale) ×
  ceil(h*scale)`; Nuklear style + font atlas rebaked on scale change;
  canvas_ity just scales its transform. VST2 has no scale protocol (assume
  1.0 or Win32 per-monitor query); VST3 `IPlugViewContentScaleSupport`; CLAP
  `set_scale`.
- **Resizing:** phase 1 fixed-size editors (`width/height` metas already in
  the DSL); phase 2 `request_resize`/`set_size` for hosts that allow it.

## 9. CMake & packaging

- `cmake/avendish.ui.soft.cmake`: FetchContent **pugl**, **Nuklear**,
  **canvas_ity** (three tiny fetches, no transitive deps), guarded like the
  Qt UI file so nothing is fetched when the backend is off.
- Backend selection: `avnd_make_vintage/vst3/clap(... UI soft)` or a global
  `AVND_UI_BACKEND=soft|qt|none` default. The glue in each binding is
  compiled only when the target type actually has a `ui` (concept-gated), so
  the zero-UI cost stays zero.
- ESP32: the runtime + canvas headers are plain C/C++ — usable directly from
  an ESP-IDF component; no cmake work beyond an example.

## 10. End-to-end example (author side, unchanged code)

`examples/Advanced/UI/Multislider.hpp` — today score-only — becomes a VST2/
VST3/CLAP plug-in with a working editor: the `struct ui` tree renders the
spinbox via `nk_property_int`, the `MultisliderWidget` custom control renders
via `soft_painter`, drags emit `begin/perform/end_edit` through its existing
`halp::transaction`, and the cursor bus messages flow over the SPSC queues.
Zero source changes to the example — that is the acceptance test.

New Tier C example `examples/Raw/NuklearDirectUi.hpp` for authors who want to
skip the DSL and write immediate-mode code themselves:

```cpp
struct ui {
  struct window {
    static constexpr auto width = 400, height = 250;
    // convenience middle tier: runtime supplies context + host, author draws
    void render(nk_context* nk) {
      if (nk_begin(nk, "main", nk_rect(0,0,400,250), 0)) {
        nk_layout_row_dynamic(nk, 120, 1);
        nk_knob_float(nk, 0.f, &gain, 1.f, 0.01f, NK_DOWN, 60.f);
      }
      nk_end(nk);
    }
    float gain{0.5f};
  };
};
```

(`soft::window<T>` detects `render(nk_context*)` and skips the layout walk.)

## 11. Implementation phases

1. **Core runtime** — `soft::ui_runtime<T>`: port the layout walk off
   GLFW/GL onto `rawfb` + fixed memory; `soft_painter` over canvas_ity;
   headless golden-image test in the existing golden harness.
2. **CLAP editor** (cleanest host API, easiest to test with `clap-host`/
   Bitwig): `clap.gui` + timer + gestures. Windows first, then X11, macOS.
3. **VST2 + VST3 glue** (small once §7.1/7.2 exist), standalone preview tool.
4. **WASM + ESP32 surfaces**; PlutoVG painter variant if canvas_ity RAM is
   prohibitive on MCU; banded rendering investigation.
5. **Polish:** HiDPI rebake, resizable editors, theming (map `halp::colors`
   palette onto `nk_style`), docs chapter under `book/src/advanced/ui.*`.

## 12. Risks / open questions

- **Nuklear HiDPI is manual** — style scaling + atlas rebake is on us
  (bounded, well-trodden problem).
- **canvas_ity memory on MCU** — internal buffers are generous; mitigation:
  PlutoVG painter variant behind the same concept (§6.3).
- **Look & feel** — stock Nuklear looks like a dev tool; a `halp::colors`-
  driven `nk_style` theme + the knob/slider skinning API needs a design pass
  to look like an instrument, not a debugger.
- **Text input/IME** on exotic hosts — Nuklear's edit widget covers ASCII
  well; full IME is explicitly out of scope for v1 (same stance as DPF).
- **First-mover**: nobody ships Nuklear inside a VST today (ImGui yes, many
  times). The rawfb/GDI demos cover ~90 % of the novel ground; fallback if it
  stalls: the L1C/L2/L3 split is unchanged if the widget layer is swapped for
  Dear ImGui + software triangle rasterizer or LVGL.

## Appendix A — survey shortlist rationale

- **Nuklear** — single C89 header, MIT/Unlicense, in-tree software rasterizer
  (`demo/rawfb`), native `nk_knob`, `nk_init_fixed` no-malloc mode, active
  (v4.13.3 May 2026). Chosen: widget layer.
- **pugl** — ISC, C99, zero deps, made precisely for plug-in editor embedding
  (`puglSetParent`), stub backend for software blit, per-instance worlds.
  Chosen: desktop embedding.
- **canvas_ity** — ISC, one header, HTML5-canvas semantics ≈ `avnd::painter`
  verbatim, software. Chosen: painter. Known ceiling: internal bitmap is
  float RGBA (16 B/px) with no banded mode — desktop-fine, MCU-hostile.
- **ctx** (ctx.graphics) — ISC (verified 2026-07: not LGPL as often
  assumed), C, actively maintained, MCU-first software rasterizer: 1–32 bpp
  incl. RGB565, tiled partial redraw, SIMD, ESP32 demos. Leading candidate
  for the MCU-tier painter behind the same concept; costs: very large
  amalgamated header, idiosyncratic API, some font gaps. PlutoVG (MIT, C99,
  4 B/px surfaces, stb_truetype) is the conservative fallback.
- **Dear ImGui** — best ecosystem & plugin precedent, but GPU-oriented (the
  maintained CPU path is SDL_Renderer; `imgui_sw` is dormant/unlicensed) and
  PSRAM-hungry on MCU. Runner-up; a later `imgui` Tier C adapter is cheap.
- **LVGL** — the embedded king (official ESP-IDF component, Emscripten port,
  MIT, extremely active), full toolkit; but big tree, retained-mode idiom of
  its own, no host-window adoption out of the box. Runner-up if priorities
  flip toward shipping MCU products.
- **microui** — 1.1 kLoC, beautiful teaching core, but dormant since 2024 and
  only rect/text/icon/clip primitives. Not enough alone.
- **raygui/TIGR/NanoGUI/sokol/Fenster/luigi/NAppGUI** — own their window/event
  loop: structurally incompatible with host-parented editors.
- **Elements** — audio-first and lovely, but Cairo + GTK3 on Linux and "not
  production ready" per its own README.
- **Slint** — royalty-free tier excludes embedded distribution; Rust toolchain
  in every cross-build.
- **GuiLite/Jet/U8g2/olive.c** — not widget toolkits in the needed sense
  (framebuffer sprites / AGPL 3D rasterizer / mono LCD / bare rasterizer).
