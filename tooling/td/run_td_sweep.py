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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--td", required=True, help="TouchDesigner.exe")
    ap.add_argument("--toe", required=True, help="driver .toe (see README.md)")
    ap.add_argument("--plugins", required=True, help="dir of built *_td.dll")
    ap.add_argument("--dumps", required=True, help="dump JSON dir")
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
    # Foo_CHOP_MESSAGE_td.dll -- both CHOP, both sanitize to the same opType, so
    # loading both just triggers "opType already used". Keep one per opType.
    fam_of = {"CHOP_AUDIO": "CHOP", "CHOP_MESSAGE": "CHOP", "TOP": "TOP",
              "SOP": "SOP", "POP": "POP", "DAT": "DAT"}
    chosen = {}
    for dll in sorted(glob.glob(os.path.join(args.plugins, "*.dll"))):
        stem = os.path.basename(dll)[:-len("_td.dll")] \
            if os.path.basename(dll).endswith("_td.dll") else os.path.basename(dll)[:-4]
        fam, base = None, stem
        for pt, f in fam_of.items():
            if stem.endswith("_" + pt):
                fam, base = f, stem[:-(len(pt) + 1)]
                break
        chosen.setdefault((base, fam or stem), dll)
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


if __name__ == "__main__":
    main()
