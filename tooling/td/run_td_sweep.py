#!/usr/bin/env python3
"""Run the avendish TouchDesigner sweep unattended and report crashes.

TouchDesigner has no headless mode, so this:
  1. stages the built Custom OP plugins into <toe folder>/Plugins/,
  2. generates the per-object runner (td_run_all.py) from the dump JSON,
  3. launches `TouchDesigner.exe <driver.toe>` (whose Execute DAT runs the
     runner via td_sweep_driver.py -- see README.md for the one-time setup),
  4. auto-dismisses the modal "Plugin Load Error" dialog that otherwise blocks
     startup (e.g. two plugins claiming the same opType),
  5. waits for td_test_report.json, then prints a pass/crash summary.

The runner instantiates + cooks every operator; an operator that crashes takes
TouchDesigner down (no report / early exit), which this flags.

Windows only (TouchDesigner + Win32 dialog handling). Example:

  python run_td_sweep.py ^
    --td "C:/Program Files/Derivative/TouchDesigner/bin/TouchDesigner.exe" ^
    --toe tooling/td/td_sweep.toe ^
    --plugins D:/build/.../td/Debug ^
    --dumps  D:/build/.../dump/all
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

user32 = ctypes.windll.user32 if sys.platform == "win32" else None

# --- Win32: find and OK the "Plugin Load Error" dialog -----------------------
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

    def _click_ok(dlg):
        # Find the child Button whose text is "OK" and BM_CLICK it.
        target = []

        def cb(hwnd, _):
            cls = ctypes.create_unicode_buffer(64)
            user32.GetClassNameW(hwnd, cls, 64)
            txt = ctypes.create_unicode_buffer(64)
            user32.GetWindowTextW(hwnd, txt, 64)
            if cls.value == "Button" and txt.value.replace("&", "").strip() == "OK":
                target.append(hwnd)
                return False
            return True
        user32.EnumChildWindows(dlg, EnumWindowsProc(cb), 0)
        if target:
            user32.SendMessageW(target[0], 0x00F5, 0, 0)  # BM_CLICK
            return True
        return False

    def dismiss_plugin_dialogs():
        n = 0
        for hwnd, title in _windows():
            if "Plugin Load Error" in title:
                if _click_ok(hwnd):
                    n += 1
        return n
else:
    def dismiss_plugin_dialogs():
        return 0


def gen_runner(dumps, out_dir):
    """Generate td_run_all.py from the dump JSON via gen_tester_patches.py."""
    here = os.path.dirname(os.path.abspath(__file__))
    gen = os.path.join(os.path.dirname(here), "gen_tester_patches.py")
    subprocess.run([sys.executable, gen, "--backend", "td-runner",
                    "--in", dumps, "--out", out_dir], check=True)
    # Depending on the gen invocation the runner lands either directly in out_dir
    # or in an td-runner/ subfolder.
    for cand in (os.path.join(out_dir, "td_run_all.py"),
                 os.path.join(out_dir, "td-runner", "td_run_all.py")):
        if os.path.exists(cand):
            return cand
    sys.exit(f"runner not generated under {out_dir}")


def compare_audio(td_out, golden, atol, rtol):
    """Diff TD output channels (positional) against the golden audio channels
    over their overlapping prefix (TD's cook sample-count can differ from the
    golden's frame count). td_out is [{name, samples}]."""
    if not golden:
        return ("no-golden-audio", "")
    if not td_out:
        return ("no-td-audio", "")
    td = [e["samples"] for e in td_out]
    nch = min(len(td), len(golden))
    if nch == 0:
        return ("empty", "")
    maxdiff = 0.0
    for c in range(nch):
        ns = min(len(td[c]), len(golden[c]))
        for i in range(ns):
            maxdiff = max(maxdiff, abs(td[c][i] - golden[c][i]))
    peak = max((abs(x) for ch in golden for x in ch), default=0.0)
    tol = atol + rtol * peak
    verdict = "match" if maxdiff <= tol else "MISMATCH"
    detail = (f"maxdiff={maxdiff:.2e} tol={tol:.2e} "
              f"td={len(td)}ch gold={len(golden)}ch")
    return (verdict, detail)


def compare_controls(named, golden, atol, rtol):
    """Diff golden output controls against TD values matched by name. `named` is
    {channel/info-name -> value}, built from output channels + Info-CHOP."""
    if not golden:
        return ("no-golden-controls", "")
    if not named:
        return ("no-td-controls", "")
    checked, maxdiff, worst = 0, 0.0, ""
    for gc in golden:
        if not isinstance(gc, dict):
            continue
        nm, gv = gc.get("name", ""), gc.get("value")
        if not isinstance(gv, (int, float)):
            continue
        tv = None
        for k in (nm, nm.lower(), nm.capitalize()):
            if k in named:
                tv = named[k]
                break
        if tv is None:
            continue
        checked += 1
        if abs(tv - gv) > maxdiff:
            maxdiff, worst = abs(tv - gv), nm
    if checked == 0:
        return ("no-name-match", f"td={list(named)[:4]}")
    peak = max((abs(gc["value"]) for gc in golden
                if isinstance(gc.get("value"), (int, float))), default=0.0)
    tol = atol + rtol * peak
    return ("match" if maxdiff <= tol else "MISMATCH",
            f"{checked} ctrls maxdiff={maxdiff:.2e}@{worst} tol={tol:.2e}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--td", required=True, help="TouchDesigner.exe")
    ap.add_argument("--toe", required=True, help="driver .toe (see README.md)")
    ap.add_argument("--plugins", required=True, help="dir of built *_td.dll")
    ap.add_argument("--dumps", required=True, help="dump JSON dir")
    ap.add_argument("--goldens", help="golden JSON dir (enables output diff)")
    ap.add_argument("--atol", type=float, default=1e-3, help="abs tolerance")
    ap.add_argument("--rtol", type=float, default=1e-3, help="rel tolerance")
    ap.add_argument("--timeout", type=int, default=300, help="seconds")
    args = ap.parse_args()

    toe = os.path.abspath(args.toe)
    toe_dir = os.path.dirname(toe)
    report = os.path.join(toe_dir, "td_test_report.json")
    errfile = os.path.join(toe_dir, "td_sweep_error.txt")

    # 1. stage plugins where TouchDesigner discovers compiled Custom OPs: the
    # top level of the user's Documents/Derivative/Plugins (TD does not recurse
    # into subfolders for DLLs, and a .toe-adjacent Plugins/ is only for .tox
    # components). Track exactly what we copy so cleanup never touches the user's
    # own plugins, and never clobber an existing file.
    plug_dir = os.path.join(os.path.expanduser("~"), "Documents", "Derivative",
                            "Plugins")
    os.makedirs(plug_dir, exist_ok=True)
    # Dedup by (object, family): a build emits e.g. Foo_CHOP_AUDIO_td.dll AND
    # Foo_CHOP_MESSAGE_td.dll -- both sanitize to the SAME opType, so loading both
    # just triggers "opType already used" and only one can register. We must stage
    # the RIGHT one: an audio object needs CHOP_AUDIO (audio_processor), a control
    # object needs CHOP_MESSAGE (message_processor). Staging CHOP_AUDIO for a
    # control object makes it report 0 output channels (no audio ports).
    def _is_audio_object(base):
        try:
            d = json.load(open(os.path.join(args.dumps, base + ".json"),
                               encoding="utf-8"))
            ports = d.get("inputs", []) + d.get("outputs", [])
            return any(p.get("type") == "audio" for p in ports)
        except Exception:
            return True  # default to the audio variant
    fam_of = {"CHOP_AUDIO": "CHOP", "CHOP_MESSAGE": "CHOP", "TOP": "TOP",
              "SOP": "SOP", "POP": "POP", "DAT": "DAT"}
    variants = {}  # (base, fam) -> {ptype: dll}
    for dll in sorted(glob.glob(os.path.join(args.plugins, "*.dll"))):
        stem = os.path.basename(dll)[:-len("_td.dll")] \
            if os.path.basename(dll).endswith("_td.dll") else os.path.basename(dll)[:-4]
        fam, base, ptype = None, stem, None
        for pt, f in fam_of.items():
            if stem.endswith("_" + pt):
                fam, base, ptype = f, stem[:-(len(pt) + 1)], pt
                break
        variants.setdefault((base, fam or stem), {})[ptype or "_"] = dll
    chosen = {}
    for (base, fam), byp in variants.items():
        if fam == "CHOP" and "CHOP_AUDIO" in byp and "CHOP_MESSAGE" in byp:
            chosen[(base, fam)] = byp["CHOP_AUDIO"] if _is_audio_object(base) \
                else byp["CHOP_MESSAGE"]
        else:
            chosen[(base, fam)] = next(iter(byp.values()))
    staged_files = []
    for dll in chosen.values():
        dst = os.path.join(plug_dir, os.path.basename(dll))
        shutil.copy2(dll, dst)  # avendish names don't collide with user plugins
        staged_files.append(dst)
    print(f"staged {len(staged_files)} plugins (deduped by opType) -> {plug_dir}")

    # 2. generate the runner.
    runner = gen_runner(args.dumps, toe_dir)
    print(f"runner: {runner}")

    # 3-5. launch, dismiss dialogs, wait for completion. If an operator crashes
    # TouchDesigner mid-sweep, record it (from the breadcrumb) and relaunch --
    # the runner resumes past everything already tested, so one bad op doesn't
    # block the rest.
    cur = os.path.join(toe_dir, "td_current.txt")
    for f in (report, errfile, cur):
        if os.path.exists(f):
            os.remove(f)
    env = dict(os.environ)
    env["AVND_TD_RUNNER"] = runner
    env["AVND_TD_REPORT_DIR"] = toe_dir
    if args.goldens:
        env["AVND_TD_GOLDEN_DIR"] = os.path.abspath(args.goldens)

    def _read(p):
        try:
            return open(p, encoding="utf-8").read().strip()
        except Exception:
            return ""

    dismissed, crashers = 0, []
    for attempt in range(1, 61):
        print(f"launch #{attempt}: {args.td} {toe}")
        proc = subprocess.Popen([args.td, toe], env=env)
        deadline, done = time.time() + args.timeout, False
        while time.time() < deadline:
            dismissed += dismiss_plugin_dialogs()
            if _read(cur) == "DONE":
                done = True
                break
            if proc.poll() is not None:
                break
            time.sleep(1)
        try:
            proc.kill()
        except Exception:
            pass
        if done:
            break
        culprit = _read(cur)
        if not culprit or culprit == "DONE":
            break  # timed out with no progress, or actually finished
        crashers.append(culprit)
        # Record the crasher so the relaunch skips it.
        try:
            rep = json.load(open(report, encoding="utf-8"))
        except Exception:
            rep = []
        if culprit not in {r["name"] for r in rep}:
            rep.append({"name": culprit, "ok": False,
                        "exception": "crashed TouchDesigner (instantiation/cook)"})
            open(report, "w", encoding="utf-8").write(json.dumps(rep, indent=2))
        print(f"  *** crashed on '{culprit}' -> recorded; relaunching to continue")

    # clean up only the plugins we staged (leave the user's own untouched).
    # TouchDesigner may still hold the DLL handles for a moment after kill, so
    # retry briefly.
    for _ in range(10):
        for f in [f for f in staged_files if os.path.exists(f)]:
            try:
                os.remove(f)
            except Exception:
                pass
        if not [f for f in staged_files if os.path.exists(f)]:
            break
        time.sleep(1)
    left = [f for f in staged_files if os.path.exists(f)]
    if left:
        print(f"warning: {len(left)} staged plugin(s) still locked in "
              f"{os.path.dirname(left[0])} -- remove manually")

    if dismissed:
        print(f"(auto-dismissed {dismissed} plugin-load dialog(s))")
    if os.path.exists(errfile):
        print("driver error:\n" + open(errfile, encoding="utf-8").read())
    if not os.path.exists(report):
        sys.exit("No report produced. See notes above.")

    results = json.load(open(report, encoding="utf-8"))
    ok = [r for r in results if r.get("ok")]
    bad = [r for r in results if not r.get("ok")]
    print(f"\n=== TouchDesigner sweep: {len(ok)}/{len(results)} ok ===")
    for r in bad:
        why = r.get("exception") or r.get("errors") or "cook error/warning"
        print(f"  FAIL {r.get('name')}: {why}")
    if crashers:
        print(f"\n*** {len(crashers)} object(s) CRASHED TouchDesigner: "
              + ", ".join(crashers) + " ***")

    # Differential: diff each captured output against the golden oracle.
    if args.goldens:
        goldens = {}
        for gf in glob.glob(os.path.join(args.goldens, "*.json")):
            try:
                g = json.load(open(gf, encoding="utf-8"))
                goldens[g.get("c_name")] = g
            except Exception:
                pass
        counts, mism = {}, []
        for r in results:
            g = goldens.get(r.get("name"))
            if not g:
                continue
            td_out = r.get("td_out")
            gaud = g.get("outputs", {}).get("audio")
            gctl = g.get("outputs", {}).get("controls")
            # Prefer audio comparison when the golden has audio output, else the
            # control outputs (which surface as named channels in TD).
            if gaud:
                v, detail = compare_audio(td_out, gaud, args.atol, args.rtol)
            elif gctl:
                # Prefer named output channels; fall back to Info-CHOP values.
                named = {}
                for e in (td_out or []):
                    if e.get("samples"):
                        named[e["name"]] = e["samples"][0]
                for k, val in (r.get("td_info") or {}).items():
                    named.setdefault(k, val)
                v, detail = compare_controls(named, gctl, args.atol, args.rtol)
            else:
                v, detail = "no-golden-output", ""
            counts[v] = counts.get(v, 0) + 1
            if v == "MISMATCH":
                mism.append(f"  MISMATCH {r['name']}: {detail}")
        print(f"\n=== output diff vs golden ({len(goldens)} goldens) ===")
        for k in sorted(counts):
            print(f"  {k}: {counts[k]}")
        for m in mism[:40]:
            print(m)


if __name__ == "__main__":
    main()
