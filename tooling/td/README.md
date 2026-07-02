# Automated TouchDesigner sweep

TouchDesigner has **no headless / command-line-script mode** — but
`TouchDesigner.exe <file>.toe` opens a project and runs its Execute-DAT
callbacks. We exploit that: a tiny **driver `.toe`** whose Execute DAT runs the
generated per-object sweep on load, and an external harness that launches it,
dismisses the startup dialog, and reads back the report.

The only manual step is creating the driver `.toe` **once** (TouchDesigner's
binary `.toe` can't be reliably synthesized by hand). After that the sweep is
fully unattended and repeatable.

## One-time: create `td_sweep.toe`

1. Launch TouchDesigner, **New** project.
2. Create an **Execute DAT** (Tab → `executeDAT`).
3. Open its callbacks (right-click → *Edit Contents*), select all, and paste the
   contents of [`td_sweep_driver.py`](td_sweep_driver.py).
4. On the Execute DAT's parameters, turn **Frame Start** = On (leave Active on).
5. **File → Save As** → `tooling/td/td_sweep.toe` (next to this README).

That driver reads two env vars (set by the harness) — `AVND_TD_RUNNER` (the
generated runner) and `AVND_TD_REPORT_DIR` — waits a few frames so every Custom
OP has registered, runs the sweep once, writes `td_test_report.json`, and quits.

## Every run: the harness

```sh
python tooling/td/run_td_sweep.py \
  --td      "C:/Program Files/Derivative/TouchDesigner/bin/TouchDesigner.exe" \
  --toe     tooling/td/td_sweep.toe \
  --plugins <build>/td/Debug \
  --dumps   <build>/dump/all
```

It will:
1. stage the `*_td.dll` into the top level of your
   `Documents/Derivative/Plugins/` (TouchDesigner loads compiled Custom OPs from
   there — **not** from a `.toe`-adjacent `Plugins/`, and it does not recurse
   into subfolders), then remove exactly what it staged afterwards. Duplicates
   are collapsed by opType: a build emits e.g. `Foo_CHOP_AUDIO_td.dll` **and**
   `Foo_CHOP_MESSAGE_td.dll`, both of which sanitize to the same opType, so only
   one is staged;
2. generate `td_run_all.py` from the dump JSON
   (`gen_tester_patches.py --backend td-runner`) — it creates each operator via
   its Python class (`sanitize_td_name(c_name)` + family, e.g. `AvndadditionCHOP`
   — TD's `create()` needs the class, not the opType string), **cooks** it,
   records `errors()`/`warnings()`, and flushes the report after every operator;
3. launch the driver `.toe`;
4. auto-dismiss the modal **"Plugin Load Error"** dialog (`BM_CLICK` its OK
   button) — raised for every duplicate/known opType conflict; without this it
   blocks startup and the sweep never runs;
5. if an operator **hard-crashes** TouchDesigner mid-sweep, the runner leaves a
   breadcrumb (`td_current.txt`) naming it; the harness records it as a crash and
   **relaunches**, and the runner resumes past everything already tested — so one
   bad op doesn't block the other 109;
6. print `N/total ok`, each per-op failure, and the list of operators that
   crashed TD.

Result classes you'll see: **ok** (instantiated + cooked clean), **"Not enough
sources specified"** (a filter/effect that needs an input wired — benign, it
instantiated fine), **class not found** (didn't load, or a family the resolver
doesn't guess), and **crashed TouchDesigner** (the real bug the sweep hunts).

## Notes
- TouchDesigner's Python is **Windows** Python: use `C:/…` paths, not MSYS
  `/c/…`.
- Registration-only smoke (does the plugin load without crashing TD) needs no
  `.toe`: drop the `*_td.dll` into `Documents/Derivative/Plugins/` and start
  TouchDesigner — if it reaches the UI, all Custom OPs registered.
