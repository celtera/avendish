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
1. stage every `*_td.dll` into `tooling/td/Plugins/` (TouchDesigner scans a
   project's `Plugins/` folder), then remove them afterwards;
2. generate `td-runner/td_run_all.py` from the dump JSON
   (`gen_tester_patches.py --backend td-runner`) — this instantiates + **cooks**
   each operator, records `errors()`/`warnings()`, and writes the report;
3. launch the driver `.toe`;
4. auto-dismiss the modal **"Plugin Load Error"** dialog (raised when two
   installed plugins claim the same opType — e.g. duplicate variants in your
   global `Documents/Derivative/Plugins/`); without this it blocks startup and
   the sweep never runs;
5. wait for `td_test_report.json` and print `N/total ok` plus each failure.

An operator that **crashes** on instantiation/cook takes TouchDesigner down, so
you get no report / an early exit — the harness flags that. Cook errors and
warnings are reported per-operator without crashing.

## Notes
- TouchDesigner's Python is **Windows** Python: use `C:/…` paths, not MSYS
  `/c/…`.
- Registration-only smoke (does the plugin load without crashing TD) needs no
  `.toe`: drop the `*_td.dll` into `Documents/Derivative/Plugins/` and start
  TouchDesigner — if it reaches the UI, all Custom OPs registered.
