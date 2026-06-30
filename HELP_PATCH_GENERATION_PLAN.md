# Automatic help / example patch generation — implementation plan

## 1. Goal

Every object that avendish builds should ship with a **help / example patch** in the
idiom of each backend, so users discover and learn an object the way they expect:

- **Pure Data** → `<c_name>-help.pd`
- **Max/MSP** → `<c_name>.maxhelp` (interactive) complementing the existing `<c_name>.maxref.xml` (reference)
- **TouchDesigner** → an example component / builder
- **Godot** → an example `.tscn` scene
- **Others (best-effort)** → Python example script, ossia/score example, vst3/clap default preset

Two sources, in priority order:

1. **Per-object override** — when registering an object, the author may point to an
   explicit, hand-authored help/example file *per backend*. It is copied verbatim to
   the right place in that backend's package.
2. **Automatically generated** — otherwise a generator produces a *rich* patch from the
   object's introspection: a titled header, a description, a **live interactive demo**
   (a sensible widget wired to each control inlet, an indicator on each outlet), and
   labelled *inlets / outlets / arguments / messages* sections with explanation text.
   When metadata is thin it falls back to a functional scaffold.

## 2. What already exists (reuse, don't reinvent)

| Piece | File | State |
|---|---|---|
| **Dump backend** → `dump/<c_name>.json` (complete introspection: metadata, ports, parameters, widgets, ranges, enum choices, messages, audio, tags) | `include/avnd/binding/dump/DumpCBOR.hpp`, `cmake/avendish.dump.cmake` (`AVND_DUMP_PATH`) | ✅ working — **this is our single source of truth** |
| **maxref pipeline**: JSON + inja template → `.maxref.xml`, copied to `max/docs/refpages/` | `examples/Demos/JSONToMaxref.cpp`, `examples/Demos/maxref_template.xml`, `cmake/avendish.max.cmake:243-263` | ✅ working — **the architectural model to follow** |
| **PD help seed** (the "started work"): templated C++ emitting `.pd` netlist; audio chain `osc~→object→clip~→dac~`, controls→`hsl`/`tgl`/`floatatom`→msg→inlet | `examples/Demos/GeneratePdHelp.cpp` | ⚠️ partial: enum/string stubbed, object hard-coded in `main()`, not in CMake |
| **Multi-backend emitter** (JSON-driven): `.maxpat`, `.pd`, TD `.py`, Godot `.gd`, Python, wasm | `tooling/gen_tester_patches.py` | ✅ working but emits *test* patches — **reference for layout/wiring logic** |
| **Packaging**: per-object registration → per-backend package assembly | `cmake/AvendishAddon.cmake` (`avnd_addon_object`), `cmake/avendish.packaging.cmake` (`avnd_addon_package`, `avnd_create_*_addon_package`) | ✅ working — **where overrides + doc copying hook in** |
| **Introspection** | `include/avnd/wrappers/metadatas.hpp`, `include/avnd/wrappers/widgets.hpp`, `include/avnd/introspection/*` | ✅ complete |

## 3. Architecture — a JSON-driven generator tool

Mirror the maxref pipeline exactly. Add **one standalone C++ tool**, built once,
invoked per object at build time, consuming `dump/<c_name>.json`:

```
examples/Demos/GeneratePatches.cpp   →  tool target `generate_patches`
  (links inja + nlohmann_json, same deps as json_to_maxref; see cmake/avendish.tools.cmake)

invocation (per object, per backend):
  generate_patches --backend pd   dump/<c_name>.json  out/pd/<c_name>-help.pd
  generate_patches --backend max  dump/<c_name>.json  out/max/help/<c_name>.maxhelp
  generate_patches --backend godot dump/<c_name>.json out/godot/examples/<c_name>.tscn
  ...
```

Why JSON-driven (not the compile-time templated approach of `GeneratePdHelp.cpp`):

- The dump JSON already captures **100%** of introspection — nothing is lost.
- **No per-object recompile** of a generator; the `*_dump` executable already exists.
- One tool, uniform across backends, integrated like `json_to_maxref`.
- Same build-dependency footprint as today (inja + nlohmann/yyjson already present).
- `GeneratePdHelp.cpp`'s widget logic and `gen_tester_patches.py`'s per-backend wiring
  port directly into the emitters.

Internal structure of the tool:

```
GeneratePatches.cpp
├── model:      parse JSON → a normalized in-memory description
│               (object{name,c_name,category,description,author,...},
│                ports[]{kind, name, widget, value_type, range{min,max,init},
│                        enum_choices[], unit, description, is_attribute},
│                messages[]{name, args[]}, audio{in_ch,out_ch,in_bus,out_bus})
├── layout:     a shared layout engine (grid/columns, coordinate allocation,
│               section dividers) — backend-agnostic geometry
├── widgets:    backend-agnostic "what control for this port" decision
│               (from widget_type + value_type + range), then per-backend realisation
└── emitters/
    ├── pd.hpp       → .pd netlist  (#N canvas / #X obj/msg/text/floatatom/connect)
    ├── max.hpp      → .maxhelp     (JSON patcher: boxes[], lines[])
    ├── td.hpp       → TD builder   (Python network-builder script + snippet)
    ├── godot.hpp    → .tscn        (Godot text scene format)
    ├── python.hpp   → example .py
    └── score.hpp    → example score / preset
```

## 4. Normalized port → widget mapping

The decision table the emitters share (driven by `widget` + `value_type` + `range` from
the dump). Each backend realises the abstract widget with its native UI object.

| Abstract control | Pd object | Max box | source signal |
|---|---|---|---|
| float slider (min/max/init) | `hsl <w> <h> <min> <max> ...` | `slider`/`live.dial` w/ range | `widget=slider/knob`, float |
| int number | `floatatom` (or `nbx`) | `number` / `live.numbox` | int |
| toggle / bool | `tgl 15 ...` | `toggle` | `widget=toggle`, bool |
| enum / combobox | message-list (`hradio` + symbol msgs, or `else/pad`-style) | `umenu` with items | `widget=combobox`, `enum_choices[]` |
| bang / impulse | `bng 15 ...` | `button` | `widget=bang` |
| string / lineedit | `symbolatom` / `msg` | `textedit` / `message` | `widget=lineedit`, string |
| color | swatch + `pack`/message triplet | `swatch` | `widget=color` |
| xy / xyz / xyzw | `nbx`×N + `pack` | `nodes` / `number`×N | `widget=xy*` |
| range slider | two `nbx` + `pack` | `rslider` | `widget=range_slider` |
| audio inlet | `osc~ 220` (demo source) | `cycle~ 220` | audio bus/channel in |
| audio outlet | `clip~ -0.95 0.95` → `dac~` / `snake~`+meter | `*~ 0.1`→`ezdac~` | audio bus/channel out |
| value/number outlet | `floatatom` / `number` sink | `number` / `print` | control output |
| message inlet (method) | `msg <name> <defaults>` → inlet | `message` → inlet | from `messages[]` |
| callback / event outlet | `print <name>` | `print <name>` | callback output |

`class_attribute`-tagged inputs (already distinguished in the dump and maxref template)
become **attributes** (Max `@attr` examples / Pd creation args), not hot inlets.

## 5. Backend-by-backend design

### 5.1 Pure Data — `<c_name>-help.pd` (PRIMARY, finishes the started work)

Format: the flat Pd netlist (`#N canvas …; #X obj …; #X msg …; #X text …; #X floatatom …;
#X connect <src> <outlet> <dst> <inlet>;`). Connection indices are object creation order
— the layout engine must track that, exactly as `GeneratePdHelp.cpp` does today.

Port the existing seed (`GeneratePdHelp.cpp`) into the `pd` emitter and complete it:

- **Header**: title bar (`cnv` with object name), an avendish/library badge (mirroring
  ELSE's header — see `Documentation/9.else/*-help.pd` for the canonical layout).
- **Description**: `#X text` block from `description` / `short_description`.
- **Live demo**: instantiate `<c_name>` (with default creation args from attributes), wire
  a widget per control inlet (table §4), `osc~` sources for audio in, `clip~`→`dac~` for
  audio out, `floatatom`/`print` on outlets.
- **Sections**: divider `cnv` bars labelled *inlets / outlets / arguments / messages*
  with one `#X text` per item: `N) <type> – <name>: <description>`.
- **Complete the stubs**: enum (emit the choice list as messages / `hradio`), string
  (`symbolatom`/message), color/xy/range (table §4).
- Optional **example subpatch** (`#N canvas … example`) for richer real-world usage when
  the object carries an `example` annotation (see §7).

Package destination: `pd/` next to the `.pd_*` external (Pd finds `<name>-help.pd` on the
path). Wire into `avnd_make_pd` like maxref is wired into `avnd_make_max`.

### 5.2 Max/MSP — `<c_name>.maxhelp`

`.maxhelp` is a **JSON patcher** (same schema as `.maxpat`): a `patcher` with `boxes[]`
(each `box` has `maxclass`, `text`, `patching_rect`, in/outlet counts) and `lines[]`
(`patchline` with `source`/`destination` `[boxid, port]`). Emit with nlohmann::json.

- Header via `comment`/`panel` boxes; description `comment`.
- Live demo: instantiate the object box, wire native UI boxes (`slider`, `live.dial`,
  `toggle`, `umenu`, `button`, `number`, `swatch`) per table §4; `cycle~`→object→`*~`→`ezdac~`
  for audio; `number`/`print` sinks on outlets.
- A `bpatcher`/tab layout section listing inlets/outlets/attributes with `comment` text.
- This **complements** the existing `.maxref.xml` (reference doc) — keep both; the
  `.maxref.xml` already ships to `max/docs/refpages/`.

Package destination: `max/help/<c_name>.maxhelp` (Max searches `help/` in a package).
Add a `help/` copy step to `avnd_create_max_addon_package` (alongside the `docs/refpages/`
copy at `avendish.packaging.cmake:114-119`).

### 5.3 TouchDesigner — example `.tox` component (RESOLVED: text-synthesis via toecollapse)

`.toe`/`.tox` are binary, **but** TouchDesigner ships official converters —
`toeexpand.exe` / `toecollapse.exe` (in `…/Derivative/TouchDesigner/bin`, present on this
machine) — that expand a `.tox` into a **plain-text directory** and collapse it back. This
was verified empirically: a `.tox` expands to `<name>.tox.dir/` + `<name>.tox.toc`, and
`toecollapse` rebuilds a valid `.tox` from that directory (round-trip confirmed; the format
is cross-platform and version-tolerant — a macOS-authored `.tox` collapsed fine on Windows).

So the primary TD path is **text synthesis** (no GUI, scriptable, CI-friendly):

1. Ship a tiny **template** expanded directory (a Base/Container COMP: `.n`, `.panel`,
   `.build`, `.toc`).
2. The `td` emitter writes, from the dump:
   - `<name>.cparm` — custom-parameter **definitions** (one per control, on pages), using
     the decoded TD param-type IDs:

     | avendish control | TD param type ID | notes |
     |---|---|---|
     | float | `772804865` | min/max/default/clamp columns |
     | int | `772804866` | |
     | toggle/bool | (toggle ID) | on/off |
     | string/lineedit | `772804868` | |
     | enum/combobox | `772804879` | trailing `N name1 "Label 1" …` choice list |
     | color (RGB/RGBA) | `772809473` | multi-component |
     | xy | `772813313` | 2 components |
     | xyz | `772813569` | 3 components |
     | TOP reference (texture) | `772804874` | for TOP operators |

   - `<name>.parm` — **init values** per control (`Name <flag> <value> [expression]`).
   - `<name>.n` — optional demo operators (an `LFO CHOP`/`Noise TOP` driving a parameter
     via an expression like `op('lfo1')['chan1']`, mirroring the sample we inspected) and a
     `Text DAT` carrying the object description + per-parameter help.
3. Run `toecollapse <name>.tox.dir` → real `<name>.tox`.

Only the small bundled `toecollapse` binary is needed at generation time (discoverable via
`TOUCHDESIGNER_SDK_PATH` / a `TOUCHDESIGNER_APP` cache var); no TD license/GUI. Guard the
step so the build still succeeds where `toecollapse` is absent (emit the `.dir` text + a
README and skip the collapse).

- **Fallback / alternative**: a TD **Python builder** script (`<c_name>.example.py`,
  extending `gen_tester_patches.py::gen_td()`) that builds the network and `comp.save()`s a
  `.tox` when run inside TD — useful for authors, not required in CI.
- **Override**: author ships a hand-built `.tox` via `EXAMPLE_TD` (§6).

Package destination: TD packages currently ship only `Plugins/`. Add an `examples/` folder
to `avnd_create_touchdesigner_package` and place the `.tox` (+ description) there.

### 5.4 Godot — example `.tscn` scene

`.tscn` **is a text format** → fully auto-generatable. Emit a scene that:

- instantiates the generated node/class (`[node name="..." type="<ClassName>"]`),
- sets a few representative exported properties from `range.init`,
- adds a `Label`/comment with the description and a minimal driver (e.g. an
  `AnimationPlayer` or script stub sweeping a parameter) where useful.

Package destination: add `examples/` (or `addons/<name>/examples/`) to
`avnd_create_godot_package`. The `.gd` logic in `gen_tester_patches.py::gen_godot()` is the
starting point.

### 5.5 Others (best-effort)

- **Python**: an example `.py` (import the module, construct, set params, run) — extend
  `gen_tester_patches.py::gen_python()`. Ship next to the `.pyd`/`.so`.
- **ossia/score**: an example `.score`/`.js` snippet (score already consumes avendish).
- **vst3 / clap**: no "patch" concept — emit a **default preset** (`.vstpreset` /
  clap state) capturing init values, plus a generated `README`/parameter list. Lowest
  priority.

## 6. Per-object override mechanism (packaging)

The natural hook is `avnd_addon_object()` (`cmake/AvendishAddon.cmake:51`) and the
single-object `avnd_make_*`. Add optional per-backend file arguments:

```cmake
avnd_addon_object(
  BASE myaddon C_NAME my_obj CLASS MyObj SOURCES MyObj.hpp
  HELP_PD      examples/help/my_obj-help.pd      # explicit Pd help
  HELP_MAX     examples/help/my_obj.maxhelp      # explicit Max help
  EXAMPLE_TD   examples/help/my_obj.tox          # explicit TD component
  EXAMPLE_GODOT examples/help/my_obj.tscn        # explicit Godot scene
)
```

Implementation:

1. Each `avnd_make_<backend>` stores the override (if given) in a target property
   (`AVND_HELP_PD`, `AVND_HELP_MAX`, `AVND_EXAMPLE_TD`, …), and otherwise registers a
   custom target that runs `generate_patches --backend <b> <dump.json> <generated>` and
   stores the generated path in the same property — exactly how `AVND_MAX_MAXREF_XML` is
   set at `avendish.max.cmake:260`.
2. `avnd_create_<backend>_addon_package` reads the property and copies **override if set,
   else generated** into the backend's help/example location (best-effort, via the
   existing `avendish.packaging.copy_optional.cmake` helper so a missing file never breaks
   the build).

This gives the requested behaviour: explicit per-object/per-backend file → expected place;
otherwise the auto-generated one.

## 7. Metadata additions to objects (optional, improves richness)

To let auto-generation be genuinely explanatory (and to make the "rich" tier shine), add a
few **optional** metadata fields read by the dump (all already follow the
`metadatas.hpp` getter-fallback pattern):

- Per-**control** `description` / `tooltip` — surfaced in the inlet/outlet section text.
  (Today only object-level `description` exists; per-control help is the main gap.)
- Per-control `step` for sliders (range currently exposes only min/max/init).
- Object-level `example` text and optional `see_also` list (for the Pd example subpatch /
  Max "see also").

These are additive; objects without them get synthesized text ("float, 0–127, default 64").

## 8. CMake / packaging integration summary

| Backend | generator output (build dir) | package destination | hook |
|---|---|---|---|
| Pd | `pd/<c_name>-help.pd` | `<pkg>/<c_name>-help.pd` | extend `avnd_make_pd` + `avnd_create_pd*_package` |
| Max | `max/help/<c_name>.maxhelp` | `<pkg>/help/<c_name>.maxhelp` | extend `avnd_make_max` + `avnd_create_max_addon_package` |
| Max (ref, existing) | `max/<c_name>.maxref.xml` | `<pkg>/docs/refpages/…` | already done |
| TouchDesigner | `td/<c_name>.example.py` (+ `.tox` override) | `<pkg>/examples/…` | extend `avnd_make_touchdesigner` + packager |
| Godot | `godot/examples/<c_name>.tscn` | `<pkg>/examples/…` | extend `avnd_make_godot` + packager |
| Python | `python/<c_name>_example.py` | next to module | extend python packaging |

All generation is **best-effort** and guarded (like `AVND_DISABLE_AUTOMAXREF`), with a
global `AVND_DISABLE_AUTOHELP` escape hatch.

## 9. Phasing

- **Phase 0 — scaffolding**: create `GeneratePatches.cpp` tool + CMake target; JSON model
  parser; shared layout engine; `--backend` dispatch. Add `AVND_*_HELP`/`AVND_EXAMPLE_*`
  target properties + override plumbing in `avnd_addon_object`/`avnd_make_*` (no emitters
  yet). *Exit: tool builds, runs, emits an empty stub per backend; overrides copy through.*
- **Phase 1 — Pure Data (finish the started work)**: full `pd` emitter — header, demo,
  sections, all widget kinds incl. the enum/string/color/xy stubs; wire into `avnd_make_pd`
  + Pd packaging. Validate generated `-help.pd` opens cleanly in Pd/plugdata for a spread of
  example objects (audio, message, controls, enum). *Exit: every Pd object ships a help patch.*
- **Phase 2 — Max `.maxhelp`**: `max` emitter (JSON patcher) + `help/` packaging; validate
  in Max. *Exit: every Max object ships `.maxhelp` + existing `.maxref.xml`.*
- **Phase 3 — Godot `.tscn`** (text, easy) **then TouchDesigner** (script + override).
- **Phase 4 — Python / score / presets** (best-effort) + per-control description metadata
  (§7) to upgrade fidelity across all emitters.

## 10. Testing & validation

- **Format validity**: parse every generated `.pd` (canvas/connect index integrity) and
  `.maxhelp` (JSON schema) in CI; for `.tscn`, load headless in Godot (the smoke-test rig
  at `avendish.godot.cmake:221` already does headless loads).
- **Open-without-error**: extend `tooling/run_backend_tests.py` to open each generated help
  patch headless (`pd --nogui`, Max via `AUTOQUIT`, Godot headless) and assert no error /
  the object instantiates.
- **Coverage report**: assert every registered example object produced (override or
  generated) a help/example for each enabled backend; `log()` any object that fell back to
  scaffold so thin metadata is visible.
- **Golden snapshots** for a few representative objects to catch layout regressions.

## 11. Open questions / risks

- ~~**TD `.tox`/`.toe` are binary**~~ **RESOLVED** — `toeexpand`/`toecollapse` (bundled with
  TD, verified locally) round-trip a `.tox` to/from plain text, so we synthesize the expanded
  directory and collapse it (§5.3). Only the small `toecollapse` binary is needed at gen time;
  remaining work is mapping the rest of the control kinds to TD param-type IDs.
- **Max `.maxhelp` fidelity** — the JSON patcher schema is large; start from a minimal
  validated skeleton (instantiate + 1 widget + 1 line) and grow.
- **Pd enum UX** — Pd has no native combobox; options are `hradio`+symbol messages or a
  message list. Pick one convention (recommend `hradio` for ≤8 choices, message list above).
- **Layout for large objects** — many controls need pagination/columns; the layout engine
  must wrap gracefully.
- **Per-control descriptions** don't exist yet (§7) — until added, "rich" text is
  synthesized from type/range/widget.
