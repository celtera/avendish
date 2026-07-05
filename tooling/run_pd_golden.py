#!/usr/bin/env python3
r"""Golden differential testing for the Pure Data binding (headless).

For every golden JSON (produced by the `golden` backend at build time), if the
object built a Pd external (``<c_name>.dll``), replay each recorded case through
a head-less Pd instance and diff the captured AUDIO output against the golden's
recorded output audio.

Mechanism (one deterministic DSP block, fresh instance per case)
----------------------------------------------------------------
Pd's default block size is 64 samples -- exactly the golden's frame count -- so
one DSP block == one golden frame. For each case we generate a driver ``.pd``
that:

  1. preloads each golden input channel into a ``[table inK 64]`` via a
     ``; inK 0 v0 v1 ... v63`` message fired on ``loadbang``;
  2. feeds the object's signal inlet(s) from those tables with
     ``[tabreceive~ inK]`` (reads one block per DSP tick);
  3. sets the input controls by sending ``<PortName> <value>`` to the left
     inlet (the audio binding routes control messages there);
  4. captures each output channel with ``[tabwrite~ outK]`` into a
     ``[table outK 64]`` -- armed by a bang BEFORE DSP is turned on so it records
     the very FIRST block from the fresh instance (matching the golden, which
     runs one block of a freshly-constructed effect);
  5. after the block has run, writes each output table to a text file
     (``; outK write <path> cr``) and quits (``; pd quit``).

Ordering is enforced with a ``[trigger]``: controls are set and ``tabwrite~`` is
armed first, DSP is enabled last, so the recorded block is block 1 with fresh
state. Each (object, case) runs in its OWN pd process for crash isolation and so
that a stateful object never carries state across cases.

Scope / honest limitations
---------------------------
Only AUDIO outputs are diffed:

  * The Pd AUDIO processor does not commit output controls (a known binding
    TODO), so audio objects that emit only control/value outputs (e.g. Peak,
    analyzers) have nothing to read back -- reported ``no-control-commit``.
  * Control-only / MIDI / texture / geometry objects have no head-less audio
    read-back path here -- reported ``no-audio-output`` (the same graceful gap
    the GStreamer harness has for shapes it cannot represent). Message objects
    do send their outputs out outlets, but robustly capturing/parsing every
    heterogeneous control-output type head-less is out of scope for this harness.

Also note the Pd audio binding defaults a *dynamic* audio bus to a single
channel, so a stereo golden is compared against Pd's channel 0 only (the
overlapping-channel prefix -- exactly what ``compare_audio`` does). Objects that
sum/route across the golden's extra input channels therefore legitimately
differ; that is reported as MISMATCH, not hidden.

Example:

  python run_pd_golden.py ^
    --pd "C:/Program Files/Pd/bin/pd.exe" ^
    --externals D:/build/build-avendish-msvc/pd/Debug ^
    --goldens D:/build/build-avendish-msvc/golden/Debug ^
    --tmp %TEMP%/pd_golden
"""

import argparse
import glob
import json
import math
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from golden_compare import aggregate_case_verdicts, compare_audio


def pd_symbol(name):
    """The symbol the Pd input binding matches a control against:
    ``pd::get_name_symbol`` keeps only [A-Za-z0-9.~] from the port name (see
    binding/pd/helpers.hpp valid_char_for_name)."""
    return re.sub(r"[^A-Za-z0-9.~]", "", str(name))


def pd_float(x):
    """Format a number as a Pd float atom (finite; non-finite -> 0)."""
    v = float(x)
    if not math.isfinite(v):
        return "0"
    return repr(v)


def build_driver(c_name, gcase, frames, tmp):
    """Return (patch_text, out_paths) for one golden case, or (None, []) if the
    case has no audio output to capture."""
    gin = gcase.get("inputs", {}).get("audio") or []
    gout = gcase.get("outputs", {}).get("audio") or []
    in_nch, out_nch = len(gin), len(gout)
    if out_nch == 0:
        return (None, [])

    lines = ["#N canvas 0 0 900 700 12;"]
    idx = 0

    def add(s):
        nonlocal idx
        lines.append(s)
        i = idx
        idx += 1
        return i

    lb = add("#X obj 20 20 loadbang;")
    # trigger: right outlet (1) = "prep" (fires first), left outlet (0) = "go".
    trig = add("#X obj 20 60 trigger bang bang;")

    trecv = [add(f"#X obj 400 {160 + 18 * k} tabreceive~ in{k};")
             for k in range(in_nch)]
    twrite = [add(f"#X obj 400 {360 + 18 * k} tabwrite~ out{k};")
              for k in range(out_nch)]
    add(f"#X obj 400 280 {c_name};")  # keep index explicit below
    # (obj index computed:)
    obj = idx - 1

    # tables holding input / output blocks
    tin = [add(f"#X obj 20 {120 + 18 * k} table in{k} {frames};")
           for k in range(in_nch)]
    tout = [add(f"#X obj 220 {120 + 18 * k} table out{k} {frames};")
            for k in range(out_nch)]

    # preload input blocks (message fired by trigger's prep outlet)
    setmsgs = []
    for k in range(in_nch):
        vals = " ".join(pd_float(x) for x in gin[k])
        setmsgs.append(add(f"#X msg 440 {120 + 18 * k} \\; in{k} 0 {vals};"))

    # control inputs (numeric; sent to the left inlet)
    cmsgs = []
    for c in gcase.get("inputs", {}).get("controls", []):
        v = c.get("value")
        if v == "unrecorded":
            continue
        if isinstance(v, bool):
            v = int(v)
        if not isinstance(v, (int, float)):
            continue  # strings/other: not sent through the audio left inlet
        cmsgs.append(add(f"#X msg 660 {60 + 22 * len(cmsgs)} "
                         f"{pd_symbol(c.get('name', ''))} {v};"))

    dspon = add("#X msg 20 460 \\; pd dsp 1;")
    d1 = add("#X obj 20 500 delay 20;")
    out_paths = [f"{tmp}/pdout_{k}.txt" for k in range(out_nch)]
    wl = "".join(f"\\; out{k} write {out_paths[k]} cr" for k in range(out_nch))
    savemsg = add(f"#X msg 20 540 {wl};")
    d2 = add("#X obj 20 580 delay 60;")
    quitmsg = add("#X msg 20 620 \\; pd quit;")

    conns = []
    # audio signal wiring
    for k in range(in_nch):
        conns.append((trecv[k], 0, obj, k))
    for k in range(out_nch):
        conns.append((obj, k, twrite[k], 0))
    # loadbang -> trigger
    conns.append((lb, 0, trig, 0))
    # prep (trigger outlet 1): preload inputs, set controls, arm tabwrite~
    for sm in setmsgs:
        conns.append((trig, 1, sm, 0))
    for cm in cmsgs:
        conns.append((trig, 1, cm, 0))
        conns.append((cm, 0, obj, 0))
    for tw in twrite:
        conns.append((trig, 1, tw, 0))
    # go (trigger outlet 0): DSP on, then delayed save + quit
    conns.append((trig, 0, dspon, 0))
    conns.append((trig, 0, d1, 0))
    conns.append((d1, 0, savemsg, 0))
    conns.append((d1, 0, d2, 0))
    conns.append((d2, 0, quitmsg, 0))

    for (s, so, d, di) in conns:
        lines.append(f"#X connect {s} {so} {d} {di};")
    return ("\n".join(lines) + "\n", out_paths)


def run_case(pd_exe, externals, c_name, gcase, meta, tmp, timeout):
    """Run one golden case head-less; return (out_channels, error).
    out_channels is [samples or None] in port order (None = channel not
    produced, e.g. Pd exposed fewer channels than the golden)."""
    frames = int(meta.get("frames", 64))
    patch, out_paths = build_driver(c_name, gcase, frames, tmp)
    if patch is None:
        return (None, "no-audio-output")

    driver = f"{tmp}/pd_driver.pd"
    with open(driver, "w", newline="\n") as f:
        f.write(patch)
    for p in out_paths:
        if os.path.exists(p):
            os.remove(p)

    cmd = [pd_exe, "-nogui", "-batch", "-noprefs",
           "-path", externals, "-open", driver]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        # A wedged pd never reaches `; pd quit`: kill any stragglers.
        subprocess.run(["taskkill", "/F", "/IM", "pd.exe"],
                       capture_output=True)
        return (None, f"timeout {timeout}s")

    # Every other object in the driver is stock Pd, so a "couldn't create" means
    # the external itself failed to instantiate. Catch it: otherwise the never-
    # written output tables read back as silence and masquerade as a (possibly
    # matching, if the golden is silent too) capture.
    if "couldn't create" in (p.stderr or ""):
        return (None, "object-not-created")

    out = []
    for p in out_paths:
        if os.path.exists(p):
            try:
                out.append([float(x) for x in open(p).read().split()])
            except Exception:
                out.append(None)
        else:
            out.append(None)
    if all(x is None for x in out):
        return (None, "no-output-captured")
    return (out, None)


def run_object(pd_exe, externals, g, atol, rtol, tmp, timeout):
    c_name = g["c_name"]
    entry = {"name": c_name, "cases": []}
    if not os.path.exists(os.path.join(externals, c_name + ".dll")):
        entry["verdict"] = "no-external"
        return entry

    gcases = g.get("cases")
    if gcases is None:  # old single-case schema
        gcases = [{"inputs": g.get("inputs", {}),
                   "outputs": g.get("outputs", {})}]
    verdicts = []
    for ci, gcase in enumerate(gcases):
        rec = {"index": ci}
        gout = gcase.get("outputs", {}).get("audio") or []
        if not gout:
            # No audio output recorded: audio processors don't commit control
            # outputs, other families have no head-less audio read-back here.
            gc = gcase.get("outputs", {}).get("controls") or []
            gin_a = gcase.get("inputs", {}).get("audio") or []
            v = "no-control-commit" if (gin_a and gc) else "no-audio-output"
            verdicts.append((ci, v, ""))
            rec["verdict"] = v
            entry["cases"].append(rec)
            continue
        out_ch, err = run_case(pd_exe, externals, c_name, gcase,
                               g.get("meta", {}), tmp, timeout)
        if err:
            verdicts.append((ci, "error", err))
            rec["verdict"], rec["error"] = "error", err
        else:
            produced = [{"name": f"c{i}", "samples": s}
                        for i, s in enumerate(out_ch) if s is not None]
            v, d = compare_audio(produced, gout, atol, rtol)
            verdicts.append((ci, v, d))
            rec["verdict"], rec["detail"] = v, d
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
    ap.add_argument("--pd", default="C:/Program Files/Pd/bin/pd.exe",
                    help="path to pd.exe")
    ap.add_argument("--externals", required=True,
                    help="dir of built pd externals (<c_name>.dll)")
    ap.add_argument("--goldens", required=True, help="golden JSON dir")
    ap.add_argument("--atol", type=float, default=1e-3, help="abs tolerance")
    ap.add_argument("--rtol", type=float, default=1e-3, help="rel tolerance")
    ap.add_argument("--report", default="pd_golden_report.json")
    ap.add_argument("--only", help="run a single object (c_name)")
    ap.add_argument("--timeout", type=int, default=30,
                    help="per-case pd invocation timeout (seconds)")
    ap.add_argument("--tmp", default=os.path.join(
        os.environ.get("TEMP", "."), "pd_golden"),
        help="scratch dir for driver patches + captured output")
    args = ap.parse_args()

    # Absolute + forward-slash: Pd resolves an array `write` path relative to the
    # patch's own directory, and a backslash gets mangled in message-box parsing.
    tmp = os.path.abspath(args.tmp).replace("\\", "/")
    os.makedirs(tmp, exist_ok=True)

    goldens = load_goldens(args.goldens)
    if args.only:
        goldens = [g for g in goldens if g["c_name"] == args.only]

    counts, mism, report = {}, [], []
    for g in goldens:
        entry = run_object(args.pd, args.externals, g, args.atol, args.rtol,
                           tmp, args.timeout)
        report.append(entry)
        v = entry.get("verdict", "?")
        counts[v] = counts.get(v, 0) + 1
        if v in ("MISMATCH", "error"):
            mism.append(f"  {v} {entry['name']} "
                        f"({len(entry.get('cases', []))} cases): "
                        f"{entry.get('detail', '')[:140]}")
        json.dump(report, open(args.report, "w"), indent=1)

    ncases = sum(len(e.get("cases", [])) for e in report)
    print(f"\n=== pd diff vs golden ({len(report)} objects, "
          f"{ncases} cases) ===")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")
    if mism:
        print("mismatches / errors:")
    for m in mism:
        print(m)
    matched = [e["name"] for e in report if e.get("verdict") == "match"]
    if matched:
        print(f"matched ({len(matched)}): " + ", ".join(matched))
    print(f"report -> {args.report}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
