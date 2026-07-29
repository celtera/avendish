# Max/MSP + Pure Data help-patch quality — improvement plan

Follow-up to `HELP_PATCH_GENERATION_PLAN.md` (which delivered the pipeline). That
plan got *a* patch out of every object; this one makes those patches good enough
that a user learns the object from them.

Everything below is measured against the full corpus: 112 dumped objects in
`examples/`, plus the external object collections in `../avendish-objects`
(`score-addon-cv`, `score-addon-onnx`, `score-addon-puara`,
`score-addon-librediffusion`).

---

## 0. Baseline — what the generator produces today

Reference build: `build-help/` (Ninja + clang-cl, Pd + Max only).
112 dumps → 98 `-help.pd`, 112 `.maxhelp`.

Static geometry check (`tooling/check_pd_patches.py`, Pd's real font metrics):

| metric | value |
|---|---|
| Pd patches with geometry problems | **84 / 98** |
| overlapping box pairs | **348** |
| boxes outside the saved canvas | **63** |
| objects carrying a description | **24 / 112** |
| ports carrying a description | **3 / 469** |

---

## 1. Root causes

### 1.1 Text overlap in Pd is a metrics bug, not a layout accident

`GeneratePatches.cpp:315` advances Y by `lines * 15` and estimates the line count
as `len / width_chars`. Both are wrong:

* Pd's font table (`C:/Program Files/Pd/tcl/pd-gui.tcl`, `font_metrics`) is
  `{5 11, 6 13, 7 16, 10 19, 14 29, 22 44}` for sizes `{8,10,12,16,24,36}`.
  At the size-12 the generator emits, a line is **16 px**, not 15 — so *every*
  consecutive comment in the inlets/outlets sections overlaps by 1 px. That alone
  is the bulk of the 348 pairs.
* The line count ignores **word wrapping**. Pd breaks a comment at the last space
  that fits in the `, f N` width; a 60-char-wide comment holding a 70-char
  sentence with a word boundary at 55 takes 2 lines, and the estimate says 2 only
  by luck. Long per-port descriptions will silently overrun.

### 1.2 There is no layout engine — only hardcoded strides

Boxes are placed at fixed offsets that assume a size they never measure:

* `emit_pd` advances the output row by `ox += 70`, but the box it just placed is
  `clip~ -0.95 0.95` — **122 px** wide. Guaranteed overlap
  (`avnd_helpers_ui-help.pd`).
* Outputs are laid out in a single unbounded horizontal row: `avnd_all_ports_types`
  reaches **x = 2064** in a canvas saved as ~620 px wide (62 boxes off-canvas).
* Canvas width/height is derived from the control count only, so anything the
  output/messages rows add falls outside.

The fix is structural: **measure every box, place into a collision-aware column
layout, and size the canvas from the resulting bounding box.**

### 1.3 The wiring does not match what the bindings actually build

This is the biggest correctness gap, and the reason output lands in `[print]`.

**Pd, message objects** (`include/avnd/binding/pd/inputs.hpp:370-409`) create
**one inlet per input port** (skipping the first port and all
`class_attribute` ports), typed `&s_list` so they accept float / symbol / list.
So a widget can connect **straight into its own inlet** — the
`[name $1(` message box the generator emits for every control is unnecessary
indirection.

**Pd, audio objects** (`include/avnd/binding/pd/audio_processor.hpp:92-102`)
create *only* signal inlets — "control_inputs is *not* initialized here, all the
messages will go through 1st inlet". Here the `[name $1(` form **is** correct.
The generator does not distinguish the two cases.

**Max, audio objects** (`include/avnd/binding/max/audio_processor.hpp:80-124`):
* `dsp_setup(&x_obj, 1)` + `Z_MC_INLETS` → **one** multichannel signal inlet,
  not one per audio port. The generator emits a `cycle~` per port wired to inlet
  `k` — inlets that do not exist.
* one `multichannelsignal` outlet, created **last** and therefore **leftmost
  (index 0)**; control outlets are created in reverse declaration order before
  it, so they sit to its right at indices `1..N` in declaration order. The
  generator uses the port index directly → signal and control lines are swapped.

**Max, message objects** (`include/avnd/binding/max/inputs.hpp:51-90`): one inlet
per **explicit** (non-attribute) parameter, with the documented shift when the
first parameter is an attribute. Attribute ports get no inlet at all.

**Outputs are not `print` material.** `value_to_pd_dispatch`
(`pd/outputs.hpp:599-637`) sends a **bare float / symbol / list** for a plain
value port, and only prefixes with a selector when the port declares
`symbol()`/`c_name()`. Max does the same (`outlet_float` / `outlet_list` vs
`outlet_anything`). So:

| output port | correct sink |
|---|---|
| float / int value port | `floatatom` (Pd) · `flonum` / `number` (Max) |
| bool | `tgl` · `toggle` |
| enum (symbol out) | `symbolatom` · `comment` fed by `route` |
| list / array / xy / rgb | `unpack`→atoms (Pd) · `unpack`→`number`s (Max) |
| declares `symbol()`/`c_name()` | `[route <name>]` **then** the sink above |
| callback with args | `[route <name>]` → `print` (genuinely message-like) |
| audio | `clip~`→`dac~`/`output~` · `*~`→`ezdac~`, plus a scope |

### 1.4 Max output ignores how Max renders

* Max **does not auto-size a `comment`**. It draws into `patching_rect` and needs
  a matching `linecount`. Every section comment is emitted 500×20 with long text
  and no `linecount` → **clipped**, which is the Max form of "text overlap".
* Attributes have a native editor, `attrui`, and a native syntax, `@name value`
  in the object box. Neither is used.
* Canonical help patches (`resources/help/max/clip.maxhelp`) use `panel` +
  styled `comment` for headers, `textcolor` for de-emphasis, and per-widget
  labels next to the widget. We emit a flat comment stack.

### 1.5 Idiom gaps vs. the platforms' own conventions

Pd vanilla (`doc/5.reference/clip-help.pd`) is the reference layout:
object + one-line description at top-left, horizontal `cnv` rules top and bottom,
a live demo in the middle with labels **beside** each widget, a `see also:` row
of clickable objects in the footer, and — crucially — the dry
inlets/outlets/arguments reference tucked into a **`pd reference` subpatch**
opened by a `<= click` hint. Putting the reference in a subpatch is what keeps
the main canvas readable; we render everything on one canvas.

### 1.6 The dump does not carry the metadata the emitters need

`include/avnd/binding/dump/DumpCBOR.hpp` is missing:

* `class_attribute` per port → can't choose inlet vs `attrui`, can't compute Max
  inlet indices.
* `symbol()` per port (`prop_symbol` does not exist in `print_metadatas`'s
  property tuple) → can't tell a prefixed outlet from a bare one.
* `unit`, and `choices` for non-enum comboboxes (`Combo_A` dumps
  `widget: combobox` with no choices).

And the objects themselves are thin: **24/112** have a description, **3/469**
ports do. The generated text is therefore mostly "auto-generated help patch".

---

## 2. Plan

### Phase 1 — dump metadata (unblocks 3, 4)
1. Add `class_attribute`, `symbol`, `unit` to the per-port dump; add `choices` for
   non-enum combobox widgets.
2. Add `prop_symbol` to `print_metadatas`'s property tuple.
3. Regenerate; assert in the checker that every parameter port resolves to a
   widget and an inlet index.

### Phase 2 — a real layout engine in `GeneratePatches.cpp`
4. A `measure`/`place` core shared by both emitters: every emitted box declares
   its `w`/`h`; Pd sizes come from the real font table + a faithful port of Pd's
   wrap algorithm; Max sizes come from Arial-12 metrics with an explicit
   `linecount`.
5. Column/section placement with explicit gutters, a running "occupied" extent,
   and canvas size derived from the final bounding box. **Invariant: no two
   foreground boxes may overlap** — enforced by the checker in CI, not by hope.
6. Wrap long control lists into columns *and* pages, so 60-parameter objects
   (`score-addon-cv`, `score-addon-onnx`) stay navigable.

### Phase 3 — Pd emitter, rebuilt on the vanilla idiom
7. Header: object box + one-line description, `cnv` rule, avendish badge.
8. Live demo: widget → **its own inlet** for message objects; `[name $1(` only
   for audio objects and attributes. Label each widget inline (name + range).
9. Outputs into real sinks per §1.3, `[route]` inserted only where the binding
   actually prefixes. `print` reserved for callbacks/messages.
10. Audio demo: `osc~`/`noise~` in, `clip~` → `output~` (vanilla's own idiom,
    with a level control) instead of a bare `dac~`.
11. Move inlets/outlets/arguments/messages text into a **`pd reference`**
    subpatch; keep `see also:` and a `pd meta` subpatch (deken convention).
12. Complete the stubbed widget kinds: enum (`hradio` + symbol messages), string,
    color, xy/xyz, range slider, multislider.

### Phase 4 — Max emitter, rebuilt on the real topology
13. Correct inlet/outlet indices per §1.3 (single MC signal inlet; signal outlet
    at 0; control outlets 1..N; explicit-parameter inlets with the attribute
    shift).
14. Native widgets: `slider`/`live.dial`/`toggle`/`umenu`/`button`/`number`/
    `flonum`/`swatch`/`multislider`, and **`attrui`** for attribute ports.
15. Every `comment` gets a measured `patching_rect` + `linecount` so nothing
    clips; `panel`-based header; `textcolor` for secondary text.
16. Sinks per §1.3 (`number`/`flonum`/`toggle`/`multislider`, `scope~`/`meter~`
    for audio) — no `print` except for callbacks.
17. Ship the object's attributes as an `@attr` example in the object box text.

### Phase 5 — validation, on the whole corpus
18. `tooling/check_pd_patches.py` (geometry + connection-index integrity) and a
    `check_max_patches.py` (JSON schema, box/line id integrity, comment fits its
    rect, inlet/outlet indices within range) run over every generated patch.
19. Runtime check: open every `-help.pd` in `pd -nogui` with the built externals
    on the path and assert no "couldn't create" / error output; drive Max via its
    own automation to load each `.maxhelp` and assert the object instantiates.
20. Run 18–19 against `../avendish-objects` too — those are the objects with
    large, awkward parameter sets that break naive layouts.

### Phase 6 — metadata in the example collection
21. Add object-level `description` to the examples that lack one, and per-port
    `description` to the demonstrative ones (`HelpersControls`, `AllPortsTypes`,
    the `Ports/` collection). This is what turns a scaffold into a help patch.

---

## 2b. Status — phases 1-5 done, measured

Implemented in `examples/Demos/GeneratePatches.cpp`,
`include/avnd/binding/dump/DumpCBOR.hpp`, `cmake/avendish.help.cmake` and
`tooling/check_help_patches.py` + `tooling/run_help_patch_smoke.py`.

| corpus | Pd geometry | Max geometry | Pd load in real Pd |
|---|---|---|---|
| `examples/` (98 pd / 112 max) | 0 issues (was 84/98 bad, 411 issues) | 0 issues (was 19) | 0/98 errors |
| `score-addon-puara` (26 / 26) | 0 | 0 | — |
| `score-addon-cv` (0 / 49) | — | 0 | — |
| `score-addon-onnx` (3 / 15) | 0 | 0 | — |

Verified in the real applications, not only statically:

* Pd 0.56 renders the header, the per-control widget+label rows, the demo chain
  and the `pd reference` subpatch with no overlap.
* Max 8 instantiates the object and renders the control column, the `umenu`
  choice lists, the audio chain (`cycle~` → object → `*~`/`ezdac~`/`scope~`) and
  the number-box sinks.
* `run_help_patch_smoke.py` loads all 98 patches in a headless Pd with the built
  externals and reports zero errors; the detector itself is proven by a negative
  test (a deliberately wrong inlet index is caught as "connection failed").

Three defects only the real applications exposed, now fixed:

1. **iemgui labels are a single atom** — `cnv ... Controls helpers ...` shifted
   every following argument, so Pd rejected the object and fell back to its
   default 100x60 geometry. Spaces must be backslash-escaped.
2. **`,` and `;` must be their own atom** — written glued to the previous word
   (`controls\,`) Pd draws the backslash; written ` \, ` it renders as plain
   `controls,`. Wrapping is computed on the *rendered* text, so the checker had
   to learn the same rule.
3. **`impulse_button` ≠ `maintained_button`** — the former holds a
   `std::optional<impulse_type>` engaged by a bare `[<name>(`, the latter a plain
   `bool`. Driving an impulse through a `$1` message box is a hard error.

Remaining: phase 6 (authoring `description()` on the objects themselves — 24/112
objects and 3/469 ports declare one today) and a Max-side automated load sweep.

## 3. Acceptance criteria

* 0 overlapping box pairs and 0 out-of-canvas boxes across every generated Pd
  patch, for `examples/` **and** the four external addons.
* 0 clipped comments in every generated `.maxhelp`; every `patchline` references
  an existing box and an in-range inlet/outlet.
* Every control has a live widget wired to the port the binding actually exposes;
  every value output lands in a number/toggle/symbol box rather than `print`.
* Both a Pd and a Max patch open cleanly with the real external loaded, with the
  object instantiating and audio running where applicable.
