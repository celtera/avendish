#!/usr/bin/env python3
"""Golden-differential testing for the avendish Max/MSP backend.

Max has NO headless mode (see run_backend_tests.py::run_max), so -- exactly like
the TouchDesigner sweep (tooling/td/run_td_sweep.py) -- this drives the GUI host:

  1. generate a self-contained data file (avnd_max_data.js) holding every
     golden's recorded inputs/outputs, so the in-Max driver needs no JSON parser;
  2. stage the driver patch + js + data next to the built *.mxe64 externals
     (Max searches the patcher's own folder, so co-location resolves both the
     externals AND the driver's include()/File paths);
  3. launch `Max.exe avnd_max_driver.maxpat`; a loadbang runs the [js] driver,
     which for every object instantiates it, replays the golden inputs and
     CAPTURES what Max can expose -- control-output objects via their message
     outlets, audio objects via a count~/index~ -> object -> record~ buffer chain
     driven by a real DSP block -- writing one JSON line per object to a report;
  4. auto-dismiss Max's modal dialogs (reuse the TD Win32 EnumWindows+BM_CLICK);
  5. poll for the report; if an object crashes Max mid-sweep, record it from the
     per-object breadcrumb and relaunch -- the driver resumes past everything
     already reported, so one bad object doesn't block the rest;
  6. diff every captured output against the golden oracle via golden_compare.

Capture reality (Max's limits, reported honestly per object):
  * control objects  -> message_processor commits output controls to outlets:
                        real value diff.
  * audio objects    -> audio_processor exposes only its MC signal outlet (it does
                        NOT commit control outlets -- known TODO), captured with
                        record~; real sample diff when DSP runs.
  * none/texture     -> nothing feasible to capture: instantiate-only smoke
                        (verdict 'instantiated' / 'no-backend-*'), never a false
                        pass.

Windows only. Example:

  C:/ossia-sdk-x86_64/python/python.exe tooling/run_max_golden.py ^
    --max "C:/Program Files/Cycling '74/Max 8/Max.exe" ^
    --externals D:/build/build-avendish-msvc/max/Debug ^
    --goldens   D:/build/build-avendish-msvc/golden/Debug
"""

import argparse
import ctypes
import glob
import json
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from golden_compare import (aggregate_case_verdicts, compare_audio,
                            compare_controls)

user32 = ctypes.windll.user32 if sys.platform == "win32" else None

# --- Win32: dismiss Max's modal dialogs (recovery, "already running", errors) --
if user32:
    EnumWindowsProc = ctypes.WINFUNCTYPE(
        ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def _windows():
        found = []

        def cb(hwnd, _):
            if not user32.IsWindowVisible(hwnd):
                return True
            buf = ctypes.create_unicode_buffer(256)
            user32.GetWindowTextW(hwnd, buf, 256)
            found.append((hwnd, buf.value))
            return True
        user32.EnumWindows(EnumWindowsProc(cb), 0)
        return found

    def _click_button(dlg, labels):
        """BM_CLICK the first child Button whose text matches one of labels."""
        target = []

        def cb(hwnd, _):
            cls = ctypes.create_unicode_buffer(64)
            user32.GetClassNameW(hwnd, cls, 64)
            txt = ctypes.create_unicode_buffer(64)
            user32.GetWindowTextW(hwnd, txt, 64)
            t = txt.value.replace("&", "").strip().lower()
            if cls.value == "Button" and t in labels:
                target.append(hwnd)
                return False
            return True
        user32.EnumChildWindows(dlg, EnumWindowsProc(cb), 0)
        if target:
            user32.SendMessageW(target[0], 0x00F5, 0, 0)  # BM_CLICK
            return True
        return False

    # Dialog titles Max raises during an automated launch, and the button we want.
    _OK = {"ok", "continue", "no", "don't save", "dont save", "close"}

    def dismiss_dialogs():
        n = 0
        for hwnd, title in _windows():
            t = (title or "").lower()
            if any(k in t for k in ("error", "max", "warning", "recover",
                                    "save", "crash", "unresolved", "missing")):
                if _click_button(hwnd, _OK):
                    n += 1
        return n
else:
    def dismiss_dialogs():
        return 0


def _round(x):
    try:
        return float(x)
    except Exception:
        return 0.0


def gen_data_js(goldens, out_path):
    """Emit avnd_max_data.js: `var AVND_DATA = [...]` -- the in-Max driver
    include()s it, so it never has to parse JSON. One entry per object with its
    kind and, per case, the input controls/audio and the golden output control
    names (so captured outlet values can be matched back to a port name)."""
    objs = []
    for g in goldens:
        cases_out = []
        cases = g.get("cases") or [{"inputs": g.get("inputs", {}),
                                    "outputs": g.get("outputs", {})}]
        kind = "none"
        for c in cases:
            ins, outs = c.get("inputs", {}), c.get("outputs", {})
            controls = [[cc.get("index", k), cc.get("name", ""), cc.get("value")]
                        for k, cc in enumerate(ins.get("controls", []))
                        if cc.get("value") != "unrecorded"
                        and isinstance(cc.get("value"), (int, float))]
            audio_in = ins.get("audio") or []
            out_ctrl_names = [cc.get("name", "")
                              for cc in outs.get("controls", [])]
            has_aout = bool(outs.get("audio"))
            has_cout = bool(outs.get("controls"))
            has_tex = bool(outs.get("texture"))
            if has_aout:
                kind = "audio"
            elif has_cout and kind != "audio":
                kind = "control"
            elif has_tex and kind not in ("audio", "control"):
                kind = "texture"
            cases_out.append({
                "controls": controls,
                "audioIn": audio_in,
                "nAudioOut": len(outs.get("audio") or []),
                "outCtrlNames": out_ctrl_names,
            })
        objs.append({"name": g["c_name"], "kind": kind, "cases": cases_out})
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("// Auto-generated by run_max_golden.py -- golden inputs/outputs\n")
        f.write("// for the in-Max driver (include()'d, so no JSON parser needed).\n")
        f.write("var AVND_DATA = ")
        json.dump(objs, f)
        f.write(";\n")
    return objs


def load_goldens(goldens_dir):
    out = []
    for f in sorted(glob.glob(os.path.join(goldens_dir, "*.json"))):
        try:
            g = json.load(open(f, encoding="utf-8"))
            if g.get("c_name"):
                out.append(g)
        except Exception:
            pass
    return out


def parse_report(path):
    """Report is JSON-lines (one object per line), written by the js driver and
    appended-to by us on crash. Returns {name: entry}."""
    entries = {}
    if not os.path.exists(path):
        return entries
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        try:
            e = json.loads(line)
            entries[e["name"]] = e
        except Exception:
            pass
    return entries


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max", required=True, help="Max.exe")
    ap.add_argument("--externals", required=True, help="dir of built *.mxe64")
    ap.add_argument("--goldens", required=True, help="golden JSON dir")
    ap.add_argument("--atol", type=float, default=1e-3)
    ap.add_argument("--rtol", type=float, default=1e-3)
    ap.add_argument("--timeout", type=int, default=240,
                    help="seconds per launch before giving up")
    ap.add_argument("--max-launches", type=int, default=80)
    ap.add_argument("--only", help="comma-separated c_names to restrict to")
    ap.add_argument("--report", default=None,
                    help="report path (default: <externals>/avnd_max_report.jsonl)")
    args = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    asset_dir = os.path.join(here, "max")
    ext_dir = os.path.abspath(args.externals).replace("\\", "/")

    goldens = load_goldens(args.goldens)
    if args.only:
        keep = set(args.only.split(","))
        goldens = [g for g in goldens if g["c_name"] in keep]
    if not goldens:
        sys.exit("no goldens matched")

    # 1. stage driver assets next to the externals (Max searches that folder).
    staged = []
    for fn in ("avnd_max_driver.maxpat", "avnd_max_driver.js"):
        dst = os.path.join(ext_dir, fn)
        shutil.copy2(os.path.join(asset_dir, fn), dst)
        staged.append(dst)
    data_js = os.path.join(ext_dir, "avnd_max_data.js")
    gen_data_js(goldens, data_js)
    staged.append(data_js)

    report = args.report or os.path.join(ext_dir, "avnd_max_report.jsonl")
    report = report.replace("\\", "/")
    breadcrumb = os.path.join(ext_dir, "avnd_max_current.txt").replace("\\", "/")
    goldir = os.path.abspath(args.goldens).replace("\\", "/")

    # config the driver reads (paths + object order).
    cfg = os.path.join(ext_dir, "avnd_max_config.js")
    with open(cfg, "w", encoding="utf-8", newline="\n") as f:
        f.write("var AVND_CFG = ")
        json.dump({"report": report, "breadcrumb": breadcrumb,
                   "goldenDir": goldir}, f)
        f.write(";\n")
    staged.append(cfg)

    for f in (report, breadcrumb):
        if os.path.exists(f):
            os.remove(f)

    patch = os.path.join(ext_dir, "avnd_max_driver.maxpat")
    kill = ["taskkill", "/F", "/IM", "Max.exe"]

    def _read(p):
        try:
            return open(p, encoding="utf-8").read().strip()
        except Exception:
            return ""

    subprocess.run(kill, capture_output=True)
    dismissed, crashers = 0, []
    for attempt in range(1, args.max_launches + 1):
        done = parse_report(report)
        remaining = [g for g in goldens if g["c_name"] not in done]
        if not remaining:
            break
        print(f"launch #{attempt}: {len(done)}/{len(goldens)} done, "
              f"{len(remaining)} remaining -> next '{remaining[0]['c_name']}'")
        proc = subprocess.Popen([args.max, patch])
        deadline = time.time() + args.timeout
        last_progress = len(done)
        stall_since = time.time()
        # Max is slow to start (~30-45s) and may hand off to another process
        # instance (so proc.poll() exiting is NOT a reason to stop). Give a
        # generous grace period until the driver first writes its breadcrumb,
        # then apply a stall timeout on lack of new objects.
        started = False
        while time.time() < deadline:
            dismissed += dismiss_dialogs()
            crumb_val = _read(breadcrumb)
            if crumb_val == "DONE":
                break
            if crumb_val and not started:
                started = True
                stall_since = time.time()
            cur = parse_report(report)
            if len(cur) != last_progress:
                last_progress = len(cur)
                stall_since = time.time()
            elif started and time.time() - stall_since > 60:
                break  # driver was running but wedged for 60s -> crash/hang
            time.sleep(1)
        try:
            proc.kill()
        except Exception:
            pass
        subprocess.run(kill, capture_output=True)
        time.sleep(2)

        if _read(breadcrumb) == "DONE":
            break
        # crashed / stalled: record the breadcrumb object so the relaunch skips it.
        culprit = _read(breadcrumb)
        base = culprit.split(":", 1)[0] if culprit else ""
        done_now = parse_report(report)
        if base and base not in done_now:
            crashers.append(culprit)
            with open(report, "a", encoding="utf-8", newline="\n") as f:
                f.write(json.dumps({"name": base, "kind": "?", "cases": [],
                                    "crash": culprit}) + "\n")
            print(f"  *** '{culprit}' crashed/stalled Max -> recorded; relaunching")
        elif not base:
            print("  no breadcrumb progress; stopping.")
            break

    subprocess.run(kill, capture_output=True)

    # clean up staged assets (leave the externals + report).
    for _ in range(8):
        left = [f for f in staged if os.path.exists(f)]
        for f in left:
            try:
                os.remove(f)
            except Exception:
                pass
        if not [f for f in staged if os.path.exists(f)]:
            break
        time.sleep(1)

    if dismissed:
        print(f"(auto-dismissed {dismissed} Max dialog(s))")

    # ---- diff vs golden -----------------------------------------------------
    goldmap = {g["c_name"]: g for g in goldens}
    entries = parse_report(report)
    counts, details, rows = {}, [], []
    for g in goldens:
        name = g["c_name"]
        e = entries.get(name)
        gcases = g.get("cases") or [{"inputs": g.get("inputs", {}),
                                     "outputs": g.get("outputs", {})}]
        if e is None:
            v, d = "no-report", "object never reported"
        elif e.get("crash"):
            v, d = "CRASH", f"crashed Max at {e.get('crash')}"
        else:
            rcases = e.get("cases", [])
            verdicts = []
            for ci, gc in enumerate(gcases):
                rc = next((r for r in rcases if r.get("index") == ci), None)
                gout = gc.get("outputs", {})
                if rc is None:
                    verdicts.append((ci, "no-case", ""))
                    continue
                if rc.get("error"):
                    verdicts.append((ci, "error", str(rc["error"])[:80]))
                    continue
                if gout.get("audio"):
                    if rc.get("dsp") is False:
                        # Max rendered no audio (no live DSP driver in this
                        # session): the external instantiated but we can't do a
                        # sample diff -- do NOT report a false mismatch.
                        verdicts.append((ci, "audio-dsp-unavailable",
                                         "instantiated; DSP did not run"))
                        continue
                    cap = rc.get("audio")
                    out = ([{"name": f"c{i}", "samples": ch}
                            for i, ch in enumerate(cap)] if cap else None)
                    verdicts.append((ci,) + compare_audio(
                        out, gout["audio"], args.atol, args.rtol))
                elif gout.get("controls"):
                    verdicts.append((ci,) + compare_controls(
                        rc.get("controls") or {}, gout["controls"],
                        args.atol, args.rtol))
                elif gout.get("texture"):
                    verdicts.append((ci, "texture-uncaptured",
                                     "jitter matrix not captured"))
                else:
                    # no output to verify: object at least instantiated + ran.
                    ok = rc.get("instantiated") or rc.get("ok")
                    verdicts.append((ci, "instantiated" if ok else "error",
                                     "" if ok else "did not instantiate"))
            v, d = aggregate_case_verdicts(verdicts)
        counts[v] = counts.get(v, 0) + 1
        rows.append((name, v, d))
        if v in ("MISMATCH", "CRASH", "error", "no-report"):
            details.append(f"  {v:10} {name}: {d}")

    print(f"\n=== Max/MSP golden diff ({len(goldens)} objects) ===")
    for k in sorted(counts):
        print(f"  {k:20} {counts[k]}")
    if crashers:
        print(f"\n*** {len(crashers)} object(s) crashed/stalled Max: "
              + ", ".join(crashers))
    if details:
        print("\n--- attention ---")
        for line in details[:60]:
            print(line)
    print(f"\nreport -> {report}")


if __name__ == "__main__":
    main()
