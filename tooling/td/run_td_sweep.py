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

    # 1. stage plugins next to the .toe so TouchDesigner discovers them.
    plug_dst = os.path.join(toe_dir, "Plugins")
    os.makedirs(plug_dst, exist_ok=True)
    staged = 0
    for dll in glob.glob(os.path.join(args.plugins, "*.dll")):
        shutil.copy2(dll, plug_dst)
        staged += 1
    print(f"staged {staged} plugins -> {plug_dst}")

    # 2. generate the runner.
    runner = gen_runner(args.dumps, toe_dir)
    print(f"runner: {runner}")

    # 3. launch TouchDesigner on the driver .toe.
    for f in (report, errfile):
        if os.path.exists(f):
            os.remove(f)
    env = dict(os.environ)
    env["AVND_TD_RUNNER"] = runner
    env["AVND_TD_REPORT_DIR"] = toe_dir
    print(f"launching: {args.td} {toe}")
    proc = subprocess.Popen([args.td, toe], env=env)

    # 4/5. dismiss dialogs, wait for the report.
    deadline = time.time() + args.timeout
    dismissed = 0
    while time.time() < deadline:
        dismissed += dismiss_plugin_dialogs()
        if os.path.exists(report):
            break
        if proc.poll() is not None and not os.path.exists(report):
            print("TouchDesigner exited before writing a report "
                  "(a Custom OP likely crashed it on instantiation).")
            break
        time.sleep(1)

    try:
        proc.kill()
    except Exception:
        pass
    # clean up staged plugins
    shutil.rmtree(plug_dst, ignore_errors=True)

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


if __name__ == "__main__":
    main()
