#!/usr/bin/env python3
"""Generate tester patches from dump JSON and run them headless, collecting a
pass/fail report per backend. The instantiation+cook smoke test catches the
crash/load-error class (e.g. a TOP that crashes on creation).

Each backend runs only if its host binary is provided:
  --python-modules DIR   run py_run_all.py with DIR on PYTHONPATH (CI-friendly)
  --pd PATH              pd -nogui -batch -open <patch> per patch (timeout)
  --max PATH             launch Max on each .maxpat (loadbang self-drives); host
                         survival = pass (Max has no real headless mode)
  --godot PATH           godot --headless on each .gd scene
  --score PATH           score headless player on each .js
  --td-note             TD: run <out>/td-runner/td_run_all.py inside TouchDesigner,
                         then re-run with --td-report <td_test_report.json> to fold in.
Writes <out>/report.json and prints a summary.
"""

import argparse
import glob
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GEN = os.path.join(HERE, "gen_tester_patches.py")


def gen_all(dumps, out):
    subprocess.run([sys.executable, GEN, "--backend", "all", "--in", dumps,
                    "--out", out], check=True)


def _launch_each(glob_pat, argv_for, timeout, label):
    """Run a host once per file; pass = clean exit within timeout, no 'error' in output."""
    results = []
    for f in sorted(glob.glob(glob_pat)):
        try:
            r = subprocess.run(argv_for(f), capture_output=True, text=True, timeout=timeout)
            out = (r.stdout + r.stderr).lower()
            results.append({"file": os.path.basename(f), "ok": r.returncode == 0
                            and "error" not in out, "exit": r.returncode})
        except subprocess.TimeoutExpired:
            results.append({"file": os.path.basename(f), "ok": False, "exit": "timeout"})
        except Exception as e:
            results.append({"file": os.path.basename(f), "ok": False, "exit": repr(e)})
    return {"backend": label, "results": results}


def run_pd(out, pd_bin, externals, dumps, timeout):
    """pd loads each self-quitting patch headless; PASS = external loaded, no
    'couldn't create'/crash. -mmio -noprefs avoids a JACK-configured pd hanging.
    Patches are written next to the externals: pd searches the patch's own dir."""
    pd_out = externals or os.path.join(out, "pd")
    subprocess.run([sys.executable, GEN, "--backend", "pd", "--autoquit",
                    "--in", dumps, "--out", pd_out], check=True)
    results = []
    for patch in sorted(glob.glob(os.path.join(pd_out, "*.pd"))):
        cn = os.path.splitext(os.path.basename(patch))[0]
        try:
            r = subprocess.run(
                [pd_bin, "-nogui", "-mmio", "-noprefs", "-verbose", "-stderr",
                 "-path", externals or pd_out, "-open", patch],
                capture_output=True, text=True, timeout=timeout)
            txt = r.stdout + r.stderr
        except subprocess.TimeoutExpired as e:
            txt = (e.stdout or "") + (e.stderr or "")
        bad = any(s in txt for s in ("couldn't create", "can't load", "sigsegv",
                                     "Access violation"))
        loaded = f"{cn}.dll and succeeded" in txt
        results.append({"object": cn, "ok": loaded and not bad,
                        "loaded": loaded, "bad": bad})
    return {"backend": "pd", "results": results}


def run_max(out, max_bin, externals, dumps, timeout):
    """Best-effort: Max is single-instance + GUI with no headless mode, so this
    launches one self-quitting patch at a time (clean exit code 0 = the external
    instantiated and Max quit; no crash). Flaky in batch (recovery dialogs); for
    a reliable signal run patches individually or interactively."""
    max_out = externals or os.path.join(out, "max")
    subprocess.run([sys.executable, GEN, "--backend", "max", "--autoquit",
                    "--in", dumps, "--out", max_out], check=True)
    kill = ["taskkill", "/F", "/IM", "Max.exe"]
    results = []
    for patch in sorted(glob.glob(os.path.join(max_out, "*.maxpat"))):
        subprocess.run(kill, capture_output=True)
        try:
            r = subprocess.run([max_bin, patch], timeout=timeout)
            ok = r.returncode == 0
        except subprocess.TimeoutExpired:
            ok = False
            subprocess.run(kill, capture_output=True)
        results.append({"object": os.path.splitext(os.path.basename(patch))[0], "ok": ok})
    return {"backend": "max", "results": results,
            "note": "flaky in batch; clean exit = instantiated + quit"}


def run_python(out, module_dir, timeout=120):
    script = os.path.join(out, "python-runner", "py_run_all.py")
    env = dict(os.environ)
    env["PYTHONPATH"] = module_dir + os.pathsep + env.get("PYTHONPATH", "")
    r = subprocess.run([sys.executable, script], env=env, capture_output=True,
                       text=True, timeout=timeout)
    return {"backend": "python", "exit": r.returncode,
            "stdout": r.stdout[-4000:], "stderr": r.stderr[-2000:]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dumps", required=True, help="dump JSON dir")
    ap.add_argument("--out", required=True, help="output dir")
    ap.add_argument("--python-modules", help="dir with built py<name> modules")
    ap.add_argument("--pd", help="pd binary")
    ap.add_argument("--pd-externals", help="dir with built <c_name>.dll pd externals")
    ap.add_argument("--max", help="Max binary")
    ap.add_argument("--max-externals", help="dir with built .mxe64 (patches go here)")
    ap.add_argument("--godot", help="godot binary")
    ap.add_argument("--score", help="score binary")
    ap.add_argument("--td-report", help="td_test_report.json produced inside TD")
    ap.add_argument("--timeout", type=int, default=20)
    args = ap.parse_args()

    gen_all(args.dumps, args.out)
    report = []

    if args.python_modules:
        report.append(run_python(args.out, args.python_modules))
    if args.pd:
        report.append(run_pd(args.out, args.pd, args.pd_externals, args.dumps, args.timeout))
    if args.max:
        report.append(run_max(args.out, args.max, args.max_externals, args.dumps, args.timeout))
    if args.godot:
        report.append(_launch_each(
            os.path.join(args.out, "godot", "*.gd"),
            lambda f: [args.godot, "--headless", "--script", f], args.timeout, "godot"))
    if args.score:
        report.append(_launch_each(
            os.path.join(args.out, "score", "*.js"),
            lambda f: [args.score, "--load-script", f], args.timeout, "score"))
    if args.td_report and os.path.exists(args.td_report):
        report.append({"backend": "touchdesigner",
                       "results": json.load(open(args.td_report, encoding="utf-8"))})

    with open(os.path.join(args.out, "report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print("=== backend test report ===")
    for b in report:
        if "results" in b:
            ok = sum(1 for r in b["results"] if r.get("ok"))
            print(f"  {b['backend']}: {ok}/{len(b['results'])} ok")
        else:
            print(f"  {b['backend']}: exit={b.get('exit')}")
    if not any(k in vars(args) and getattr(args, k) for k in
               ("python_modules", "pd", "max", "godot", "score", "td_report")):
        print("  (no host binaries given: patches generated only)")


if __name__ == "__main__":
    main()
