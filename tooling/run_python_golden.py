#!/usr/bin/env python3
"""Golden differential testing for the Python binding (in-process).

For every golden JSON (produced by the `golden` backend at build time), import
the object's Python module (py<c_name>.pyd), replay each recorded test case --
set the input controls as attributes, feed the recorded audio through
process_audio() or call process() -- capture the outputs (control attributes,
audio channels, CPU textures) and diff them against the golden's recorded
outputs. A fresh instance is created per case, matching the golden generator's
fresh-effect-per-case semantics.

A crashing native module takes its whole process down, so the default (parent)
mode spawns one --child subprocess per object: a crash costs that object only,
and the child's stage breadcrumb (construct/apply/run/read) says where it died.

Example:

  python run_python_golden.py ^
    --modules D:/build/build-avendish-msvc/python/Debug ^
    --goldens D:/build/build-avendish-msvc/golden/Debug
"""

import argparse
import glob
import importlib
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from golden_compare import (aggregate_case_verdicts, compare_audio,
                            compare_controls, compare_textures, fnv1a64)


def norm(name):
    """Names differ per backend (golden records the port's display name, the
    python binding uses its C identifier): compare lowercase alphanumerics."""
    return re.sub(r"[^a-z0-9]", "", str(name).lower())


def find_class(mod, c_name):
    cls = getattr(mod, c_name, None)
    if isinstance(cls, type):
        return cls
    for v in vars(mod).values():
        if isinstance(v, type) and getattr(v, "__module__", "") == mod.__name__:
            return v
    return None


def attr_map(obj):
    return {norm(a): a for a in dir(obj) if not a.startswith("_")}


def set_control(obj, attrs, name, value):
    """Set one golden input control on the instance. Returns (ok, error)."""
    a = attrs.get(norm(name))
    if a is None:
        return (False, "no-attr")
    try:
        setattr(obj, a, value)
        return (True, None)
    except Exception:
        pass
    # Enum properties reject raw ints; construct the bound enum type.
    try:
        cur = getattr(obj, a)
        if hasattr(type(cur), "__members__"):
            setattr(obj, a, type(cur)(int(value)))
            return (True, None)
    except Exception as e:
        return (False, str(e))
    return (False, "type-mismatch")


def capture_textures(obj, attrs, count):
    """Read CPU-texture outputs: ndarray-valued attributes, hashed exactly like
    the golden generator (FNV-1a over the raw row-major pixel bytes)."""
    import numpy as np
    if count <= 0:
        return []
    textures = []
    for a in sorted(attrs.values()):
        try:
            v = getattr(obj, a)
        except Exception:
            continue
        if isinstance(v, np.ndarray) and v.ndim == 3:
            textures.append({
                "width": int(v.shape[1]),
                "height": int(v.shape[0]),
                "hash": fnv1a64(np.ascontiguousarray(v).tobytes()),
            })
    return textures


def run_case(cls, gcase, meta, stage):
    """Replay one golden case on a fresh instance; returns the case record."""
    import numpy as np
    rec = {"ok": False, "applied": {}, "skipped": []}
    stage("construct")
    obj = cls()
    attrs = attr_map(obj)

    stage("apply")
    for c in gcase.get("inputs", {}).get("controls", []):
        nm, val = c.get("name", ""), c.get("value")
        if val == "unrecorded":
            continue
        ok, err = set_control(obj, attrs, nm, val)
        if ok:
            rec["applied"][nm] = val
        else:
            rec["skipped"].append(f"{nm}:{err}")

    gin = gcase.get("inputs", {}).get("audio") or []
    frames = int(meta.get("frames", 64))
    rate = float(meta.get("rate", 44100.0))
    stage("run")
    if hasattr(obj, "process_audio"):
        arr = (np.asarray(gin, dtype=np.float32) if gin
               else np.zeros((0, frames), dtype=np.float32))
        out = obj.process_audio(arr, rate)
        rec["audio"] = [{"name": f"c{i}", "samples": [float(x) for x in ch]}
                        for i, ch in enumerate(np.asarray(out))]
    elif hasattr(obj, "process"):
        obj.process()
    else:
        rec["runner"] = "none"
        return rec

    stage("read")
    named = {}
    for gc in gcase.get("outputs", {}).get("controls", []):
        nm = gc.get("name", "")
        # Unnamed output ports: golden says p<i>, the python binding disambi-
        # guates them from unnamed inputs as out_p<i>. Prefer the out_ form so
        # an unnamed input p<i> attribute doesn't shadow the output we want.
        a = attrs.get(norm("out_" + nm)) or attrs.get(norm(nm))
        if a is None:
            continue
        try:
            v = getattr(obj, a)
        except Exception:
            continue
        if isinstance(v, (bool, int, float)):
            named[gc["name"]] = float(v)
        elif isinstance(v, str):
            named[gc["name"]] = v
    rec["controls"] = named
    rec["textures"] = capture_textures(
        obj, attrs, len(gcase.get("outputs", {}).get("texture", [])))
    rec["ok"] = True
    return rec


def compare_case(rec, gout, atol, rtol):
    """Pick the comparison matching what the golden recorded for this case."""
    if rec.get("runner") == "none":
        return ("no-runner", "")
    if gout.get("audio"):
        return compare_audio(rec.get("audio"), gout["audio"], atol, rtol)
    if gout.get("controls"):
        return compare_controls(rec.get("controls"), gout["controls"], atol, rtol)
    if gout.get("texture"):
        return compare_textures(rec.get("textures"), gout["texture"], atol, rtol)
    return ("no-golden-output", "")


def run_object(g, atol, rtol, stagefile):
    """Import one object's module and replay all its golden cases."""
    c_name = g["c_name"]

    def stage(s):
        open(stagefile, "w").write(f"{c_name}:{s}")

    entry = {"name": c_name, "cases": []}
    stage("import")
    try:
        mod = importlib.import_module("py" + c_name)
    except ImportError:
        entry["verdict"] = "no-module"
        return entry
    cls = find_class(mod, c_name)
    if cls is None:
        entry["verdict"] = "no-class"
        return entry

    gcases = g.get("cases")
    if gcases is None:  # old single-case schema
        gcases = [{"inputs": g.get("inputs", {}),
                   "outputs": g.get("outputs", {})}]
    verdicts = []
    for ci, gcase in enumerate(gcases):
        try:
            rec = run_case(cls, gcase, g.get("meta", {}),
                           lambda s: stage(f"case{ci}:{s}"))
        except Exception as e:
            rec = {"ok": False, "exception": str(e)}
        rec["index"] = ci
        if "exception" in rec:
            verdicts.append((ci, "exception", rec["exception"]))
        else:
            v, d = compare_case(rec, gcase.get("outputs", {}), atol, rtol)
            verdicts.append((ci, v, d))
            rec["verdict"] = v
        entry["cases"].append(rec)
    v, detail = aggregate_case_verdicts(verdicts)
    entry["verdict"] = v
    entry["detail"] = detail
    return entry


def load_goldens(goldens_dir):
    goldens = []
    for f in sorted(glob.glob(os.path.join(goldens_dir, "*.json"))):
        try:
            g = json.load(open(f))
            if g.get("c_name"):
                goldens.append(g)
        except Exception:
            pass
    return goldens


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--modules", required=True, help="dir of built py*.pyd")
    ap.add_argument("--goldens", required=True, help="golden JSON dir")
    ap.add_argument("--atol", type=float, default=1e-3, help="abs tolerance")
    ap.add_argument("--rtol", type=float, default=1e-3, help="rel tolerance")
    ap.add_argument("--report", default="py_golden_report.json")
    ap.add_argument("--only", help="run a single object (c_name)")
    ap.add_argument("--child", action="store_true",
                    help="internal: run --only's object in this process")
    ap.add_argument("--timeout", type=int, default=120,
                    help="per-object subprocess timeout (seconds)")
    args = ap.parse_args()

    sys.path.insert(0, args.modules)
    goldens = load_goldens(args.goldens)
    if args.only:
        goldens = [g for g in goldens if g["c_name"] == args.only]

    if args.child:
        if not goldens:
            return 2
        entry = run_object(goldens[0], args.atol, args.rtol,
                           args.report + ".stage")
        json.dump(entry, open(args.report, "w"), indent=1)
        return 0

    counts, mism, report = {}, [], []
    for g in goldens:
        c_name = g["c_name"]
        objreport = args.report + ".obj"
        stagefile = objreport + ".stage"
        for f in (objreport, stagefile):
            if os.path.exists(f):
                os.remove(f)
        cmd = [sys.executable, os.path.abspath(__file__),
               "--modules", args.modules, "--goldens", args.goldens,
               "--atol", str(args.atol), "--rtol", str(args.rtol),
               "--report", objreport, "--only", c_name, "--child"]
        try:
            p = subprocess.run(cmd, capture_output=True, text=True,
                               timeout=args.timeout)
            crashed = p.returncode != 0 or not os.path.exists(objreport)
            how = f"exit {p.returncode}"
        except subprocess.TimeoutExpired:
            crashed = True
            how = f"timeout {args.timeout}s"
        if crashed:
            st = ""
            if os.path.exists(stagefile):
                st = open(stagefile).read()
            entry = {"name": c_name, "verdict": "CRASH",
                     "detail": f"{how} at {st or 'startup'}"}
        else:
            entry = json.load(open(objreport))
        report.append(entry)
        v = entry.get("verdict", "?")
        counts[v] = counts.get(v, 0) + 1
        if v in ("MISMATCH", "CRASH", "exception"):
            mism.append(f"  {v} {c_name} ({len(entry.get('cases', []))} cases):"
                        f" {entry.get('detail', '')[:140]}")
        json.dump(report, open(args.report, "w"), indent=1)

    ncases = sum(len(e.get("cases", [])) for e in report)
    print(f"\n=== python diff vs golden ({len(report)} objects, "
          f"{ncases} cases) ===")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")
    for m in mism:
        print(m)
    print(f"report -> {args.report}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
