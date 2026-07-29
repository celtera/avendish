#!/usr/bin/env python3
"""Open every generated Pure Data help patch in a headless Pd and report errors.

A help patch that passes the static checks can still be wrong at load time: if a
connection targets an inlet the external does not create, Pd drops it and prints
an error. This walks the patches, loads each one with the built externals on the
search path, and fails on any Pd-reported problem.

    py run_help_patch_smoke.py --pd-dir build/pd --pd-exe "C:/Program Files/Pd/bin/pd.com"
"""

import argparse
import concurrent.futures
import glob
import os
import re
import subprocess
import sys

# Pd chatters about audio backends it cannot open; that is not a patch problem.
NOISE = re.compile(
    r"jack|JackShm|Audio input error|Audio output error|ALSA|portaudio|"
    r"^\s*$|input channels|output channels|audio I/O error|"
    r"couldn't set sample rate|opened .* device", re.I)

# What we do care about.
# "connection failed" is the one Pd prints when a patch wires an inlet/outlet
# the object does not actually create -- exactly what a wrong topology produces.
FAILURE = re.compile(
    r"connection failed|couldn't create|can't connect|connect: |no such object|"
    r"^error:|\berror\b.*(inlet|outlet|connect)|signal outlet connect|"
    r"expected .* but got|multiply defined", re.I)


def run_one(pd_exe, patch, path_dirs, timeout):
    cmd = [pd_exe, "-nogui", "-noprefs", "-nosound", "-mmio", "-stderr"]
    for d in path_dirs:
        cmd += ["-path", d]
    cmd += ["-open", patch, "-send", "pd quit"]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        out = (r.stdout or "") + (r.stderr or "")
    except subprocess.TimeoutExpired:
        return patch, ["TIMEOUT after %ds" % timeout]
    except OSError as e:                                       # noqa: BLE001
        return patch, ["cannot run pd: %s" % e]

    bad = []
    for line in out.splitlines():
        line = line.strip()
        if not line or NOISE.search(line):
            continue
        if FAILURE.search(line):
            bad.append(line)
    return patch, bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pd-dir", required=True,
                    help="directory with the generated *-help.pd and the externals")
    ap.add_argument("--pd-exe", default="pd",
                    help="Pd executable (use pd.com on Windows for console output)")
    ap.add_argument("--path", action="append", default=[],
                    help="extra -path entries")
    ap.add_argument("--timeout", type=int, default=40)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    patches = sorted(glob.glob(os.path.join(args.pd_dir, "*-help.pd")))
    if not patches:
        print("no help patches found in %s" % args.pd_dir)
        return 1
    dirs = [os.path.abspath(args.pd_dir)] + [os.path.abspath(p) for p in args.path]

    failures = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futs = [ex.submit(run_one, args.pd_exe, p, dirs, args.timeout)
                for p in patches]
        for fut in concurrent.futures.as_completed(futs):
            patch, bad = fut.result()
            if bad:
                failures += 1
                print("%-52s %d error(s)" % (os.path.basename(patch), len(bad)))
                for line in bad[:8]:
                    print("    " + line)
            elif args.verbose:
                print("%-52s ok" % os.path.basename(patch))

    print("\n%d/%d help patches loaded with errors" % (failures, len(patches)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
