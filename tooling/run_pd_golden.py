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

Control / message objects (no DSP)
----------------------------------
Objects without audio I/O run through the Pd MESSAGE processor: input controls
are set by ``<PortName> <value>`` messages (impulse ports by a selector-only
``<PortName>`` message, unnamed ports by their positional ``p<index>``
selector), declared message ports are replayed from the golden's recorded
``inputs.messages``, and a final ``bang`` runs the object once and commits
every output control to its outlet. Each outlet is wired to ``[print cap<k>]``
and the values are parsed back from pd's stdout (pd -nogui prints there).
All the control sends + the bang live in ONE comma-separated message box, so
their order is guaranteed. Outlet -> port mapping comes from the introspection
dump (``--dumps``, declaration order); without a dump, positional mapping is
used when the object has only one kind of capturable output.

Audio objects additionally get the same control-input treatment (strings,
impulses, messages), and -- since the audio binding now commits non-audio
outputs on bang -- a ``bang`` is sent after the recorded block and any control
or callback outputs are captured through ``[print]`` and diffed too (Peak-style
analyzers). The control values are read after ~13 blocks of the same repeating
input, so only block-stable control outputs can match; stateful accumulators
legitimately differ and are reported as MISMATCH.

Remaining honest gaps
---------------------
  * MIDI / texture / geometry / buffer content is not representable in a Pd
    message capture; such objects report their instantiation status only.
  * The Pd audio binding defaults a *dynamic* audio bus to a single channel,
    so a stereo golden is compared against Pd's channel 0 only (the
    overlapping-channel prefix -- exactly what ``compare_audio`` does). Objects
    that sum/route across the golden's extra input channels legitimately
    differ; that is reported as MISMATCH, not hidden.
  * For audio objects the control-outlet indices assume Pd exposed as many
    signal outlets as the golden recorded; dynamic-bus objects where that
    differs miss their control capture (reported, not hidden).

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
from golden_compare import (aggregate_case_verdicts, compare_audio,
                            compare_callbacks, compare_controls)


def pd_symbol(name):
    """The symbol the Pd input binding matches a control against:
    ``pd::get_name_symbol`` REPLACES every character outside [A-Za-z0-9.~]
    with '_' (avnd::fixup_identifier with pd::valid_char_for_name) -- e.g.
    port "Combo_A" stays "Combo_A", "with space" becomes "with_space"."""
    return re.sub(r"[^A-Za-z0-9.~]", "_", str(name))


def pd_float(x):
    """Format a number as a Pd float atom (finite; non-finite -> 0)."""
    v = float(x)
    if not math.isfinite(v):
        return "0"
    return repr(v)


def pd_atom(v):
    """One golden value as a Pd message atom."""
    if isinstance(v, bool):
        return str(int(v))
    if isinstance(v, float):
        return pd_float(v)
    return str(v)


def control_segments(gcase):
    """The ordered message segments that replay this case's control inputs and
    message invocations; joined with commas they form one Pd message box whose
    parts fire sequentially (guaranteed order)."""
    segs = []
    for c in gcase.get("inputs", {}).get("controls", []):
        v = c.get("value")
        nm = pd_symbol(c.get("name", ""))
        if not nm or v == "unrecorded" or v is None:
            continue
        if v == "bang":
            segs.append(nm)  # selector-only message engages the impulse
        elif isinstance(v, list):
            # container control (vector / array / aggregate): one list message
            if all(isinstance(x, (bool, int, float, str)) for x in v):
                segs.append((nm + " " + " ".join(pd_atom(x) for x in v)).strip())
        elif isinstance(v, (bool, int, float, str)):
            segs.append(f"{nm} {pd_atom(v)}")
    for m in gcase.get("inputs", {}).get("messages", []) or []:
        if m.get("status") != "invoked":
            continue
        nm = pd_symbol(m.get("name", ""))
        if not nm:
            continue
        args = " ".join(pd_atom(a) for a in m.get("args", []))
        segs.append(f"{nm} {args}".strip())
    return segs


def load_dump(dumps_dir, stem):
    """Declaration-ordered output ports [{name, type}] from the introspection
    dump JSON for this object, or None if unavailable."""
    if not dumps_dir or not stem:
        return None
    p = os.path.join(dumps_dir, stem + ".json")
    if not os.path.exists(p):
        return None
    try:
        outs = json.load(open(p)).get("outputs")
        return outs if isinstance(outs, list) else []
    except Exception:
        return None


def parse_prints(proc):
    """Parse ``[print cap<k>]`` lines from a finished pd process into
    {outlet_index: [event, ...]} where each event is a list of atoms
    (floats where parseable, else strings). Windows pd -nogui routes [print]
    to stderr; scan both streams."""
    caps = {}
    text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    for line in text.splitlines():
        m = re.match(r"^cap(\d+): (.*)$", line.strip())
        if not m:
            continue
        k, rest = int(m.group(1)), m.group(2).strip()
        for prefix in ("symbol ", "list ", "float "):
            if rest.startswith(prefix):
                rest = rest[len(prefix):]
                break
        vals = []
        if rest != "bang":
            # Pd escapes spaces inside a single symbol atom ("from\ bang"):
            # split on unescaped whitespace only, then unescape.
            for t in re.split(r"(?<!\\)\s+", rest):
                if not t:
                    continue
                t = t.replace("\\ ", " ").replace("\\,", ",").replace("\\;", ";")
                try:
                    vals.append(float(t))
                except ValueError:
                    vals.append(t)
        caps.setdefault(k, []).append(vals)
    return caps


def out_port_map(outs_decl, gcase, audio_obj):
    """Map capturable output ports to print-outlet indices.
    Returns (params, callbacks, err) where params is [(cap_idx, name)] for
    parameter outputs and callbacks is [(cap_idx, name)] for callback outputs;
    cap_idx is the index of the [print cap<k>] the port is wired to (for audio
    objects the harness wires prints only to the non-audio outlets, so
    cap indices count non-audio ports in declaration order)."""
    gctl = gcase.get("outputs", {}).get("controls") or []
    gcb = gcase.get("outputs", {}).get("callbacks") or []
    if outs_decl is not None:
        params, callbacks, k = [], [], 0
        for o in outs_decl:
            ty = o.get("type", "")
            if audio_obj and ty == "audio":
                continue  # signal outlets are not wired to prints
            nm = o.get("name")
            if ty == "callback":
                callbacks.append((k, nm))
            elif ty == "parameter":
                params.append((k, nm))
            # other types (midi, texture...) still occupy an outlet
            k += 1
        # unnamed ports: fall back to the golden's positional naming
        params = [(k, nm if nm else f"p{i}") for i, (k, nm) in enumerate(params)]
        callbacks = [(k, nm if nm else f"p{i}")
                     for i, (k, nm) in enumerate(callbacks)]
        return (params, callbacks, None)
    # No dump: positional mapping is only unambiguous with one kind of output.
    if gcb and gctl:
        return ([], [], "ambiguous-outlets")
    if gcb:
        return ([], [(i, cb.get("name", f"p{i}")) for i, cb in enumerate(gcb)],
                None)
    return ([(i, c.get("name", f"p{i}")) for i, c in enumerate(gctl)], [], None)


def build_driver(c_name, gcase, frames, tmp, n_prints=0):
    """Audio-object driver: return (patch_text, out_paths) for one golden case,
    or (None, []) if there is nothing to capture (no audio output and no
    control capture requested). n_prints control/callback outlets -- assumed to
    sit after the signal outlets -- are wired to [print cap<k>] and flushed by
    a bang sent after the recorded block."""
    gin = gcase.get("inputs", {}).get("audio") or []
    gout = gcase.get("outputs", {}).get("audio") or []
    in_nch, out_nch = len(gin), len(gout)
    if out_nch == 0 and n_prints == 0:
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

    # control inputs / message replays: ONE comma-separated message box so the
    # sends happen in golden order (multiple connections from one outlet have
    # no guaranteed order in Pd).
    segs = control_segments(gcase)
    ctlmsg = add("#X msg 660 60 " + " \\, ".join(segs) + ";") if segs else None

    # prints capturing the control/callback outlets
    prints = [add(f"#X obj 660 {200 + 22 * k} print cap{k};")
              for k in range(n_prints)]
    bangmsg = add("#X msg 20 440 bang;") if n_prints else None

    dspon = add("#X msg 20 460 \\; pd dsp 1;")
    # One-block gate: [bang~] fires at the end of every DSP block; a counter +
    # [sel 1] triggers exactly once, right after the FIRST block -- so control
    # outputs are committed with the same one-block state the golden recorded
    # (a wall-clock delay would let ~13 blocks run first).
    bng = add("#X obj 700 400 bang~;")
    fcnt = add("#X obj 700 430 f 1;")
    finc = add("#X obj 760 430 + 1;")
    fsel = add("#X obj 700 460 sel 1;")
    seq = add("#X obj 700 490 t b b b;")
    out_paths = [f"{tmp}/pdout_{k}.txt" for k in range(out_nch)]
    wl = "".join(f"\\; out{k} write {out_paths[k]} cr" for k in range(out_nch))
    savemsg = add(f"#X msg 20 540 {wl};") if out_nch else None
    d2 = add("#X obj 20 580 delay 30;")
    quitmsg = add("#X msg 20 620 \\; pd quit;")
    # watchdog: quit even if DSP never produces a block
    wd = add("#X obj 220 580 delay 2000;")

    conns = []
    # audio signal wiring
    for k in range(in_nch):
        conns.append((trecv[k], 0, obj, k))
    for k in range(out_nch):
        conns.append((obj, k, twrite[k], 0))
    # Control/callback outlets follow the signal outlets; the binding creates
    # one signal outlet per audio output channel (none for analyzers), which
    # matches the golden's recorded channel count.
    for k in range(n_prints):
        conns.append((obj, out_nch + k, prints[k], 0))
    # loadbang -> trigger
    conns.append((lb, 0, trig, 0))
    # prep (trigger outlet 1): preload inputs, set controls, arm tabwrite~
    for sm in setmsgs:
        conns.append((trig, 1, sm, 0))
    if ctlmsg is not None:
        conns.append((trig, 1, ctlmsg, 0))
        conns.append((ctlmsg, 0, obj, 0))
    for tw in twrite:
        conns.append((trig, 1, tw, 0))
    # go (trigger outlet 0): DSP on; the [bang~]->counter->[sel 1] chain fires
    # once at the end of the first block, then (right-to-left trigger order)
    # commit-bang -> save -> quit.
    conns.append((trig, 0, dspon, 0))
    conns.append((bng, 0, fcnt, 0))
    conns.append((fcnt, 0, finc, 0))
    conns.append((finc, 0, fcnt, 1))
    conns.append((fcnt, 0, fsel, 0))
    conns.append((fsel, 0, seq, 0))
    if bangmsg is not None:
        conns.append((seq, 2, bangmsg, 0))
        conns.append((bangmsg, 0, obj, 0))
    if savemsg is not None:
        conns.append((seq, 1, savemsg, 0))
    conns.append((seq, 0, d2, 0))
    conns.append((d2, 0, quitmsg, 0))
    # watchdog quit if no DSP block ever completes
    conns.append((trig, 0, wd, 0))
    conns.append((wd, 0, quitmsg, 0))

    for (s, so, d, di) in conns:
        lines.append(f"#X connect {s} {so} {d} {di};")
    return ("\n".join(lines) + "\n", out_paths)


def build_control_driver(c_name, gcase, n_outlets):
    """Message-object driver: set the case's controls, replay its messages,
    bang once (runs the object and commits every output control), capture all
    n_outlets outlets with [print cap<k>], quit."""
    lines = ["#N canvas 0 0 900 700 12;"]
    idx = 0

    def add(s):
        nonlocal idx
        lines.append(s)
        i = idx
        idx += 1
        return i

    lb = add("#X obj 20 20 loadbang;")
    trig = add("#X obj 20 60 trigger bang bang;")
    obj = add(f"#X obj 20 220 {c_name};")
    prints = [add(f"#X obj {20 + 130 * k} 320 print cap{k};")
              for k in range(n_outlets)]
    # all sends in ONE comma-separated message box: guaranteed order,
    # ending with the bang that runs + commits.
    segs = control_segments(gcase) + ["bang"]
    msg = add("#X msg 320 60 " + " \\, ".join(segs) + ";")
    d1 = add("#X obj 20 100 delay 50;")
    quitmsg = add("#X msg 20 140 \\; pd quit;")

    conns = [(lb, 0, trig, 0), (trig, 1, msg, 0), (msg, 0, obj, 0),
             (trig, 0, d1, 0), (d1, 0, quitmsg, 0)]
    for k, pr in enumerate(prints):
        conns.append((obj, k, pr, 0))
    for (s, so, d, di) in conns:
        lines.append(f"#X connect {s} {so} {d} {di};")
    return "\n".join(lines) + "\n"


def launch_pd(pd_exe, externals, patch_text, tmp, timeout):
    """Write the driver patch, run pd head-less; return (proc, error)."""
    driver = f"{tmp}/pd_driver.pd"
    with open(driver, "w", newline="\n") as f:
        f.write(patch_text)
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
    return (p, None)


def compare_captured_controls(caps, params, callbacks, gcase, atol, rtol):
    """Diff the parsed [print] captures against the golden's output controls
    and callback events; returns a list of (verdict, detail)."""
    results = []
    gctl = gcase.get("outputs", {}).get("controls") or []
    gcb = gcase.get("outputs", {}).get("callbacks") or []
    recordable = [c for c in gctl
                  if isinstance(c, dict) and c.get("value") != "unrecorded"]
    if recordable and params:
        named = {}
        for cap_idx, nm in params:
            events = caps.get(cap_idx) or []
            if events and events[-1]:
                ev = events[-1]
                named[nm] = ev[0] if len(ev) == 1 else ev
        results.append(compare_controls(named, gctl, atol, rtol))
    for i, (cap_idx, nm) in enumerate(callbacks):
        gev = None
        for cb in gcb:
            if cb.get("name") == nm or cb.get("index") == i:
                gev = cb.get("events")
                break
        if gev is None:
            continue
        events = caps.get(cap_idx) or []
        v, d = compare_callbacks(events, gev, atol, rtol)
        if v == "MISMATCH" and events and all(e for e in events):
            # Callbacks with a symbol/c_name are emitted [<sym> args( in Pd;
            # the golden records only the args. Retry with the leading symbol
            # stripped when every event starts with the same non-golden token.
            stripped = [e[1:] for e in events]
            v2, d2 = compare_callbacks(stripped, gev, atol, rtol)
            if v2 == "match":
                v, d = v2, d2 + " (symbol-prefixed)"
        results.append((v, f"cb {nm}: {d}"))
    return results


def run_audio_case(pd_exe, externals, c_name, gcase, meta, tmp, timeout,
                   outs_decl, atol, rtol):
    """One golden case of an audio object: run DSP, capture audio tables and
    (via bang + [print]) any control/callback outputs. Returns
    (verdict, detail)."""
    frames = int(meta.get("frames", 64))
    gout = gcase.get("outputs", {}).get("audio") or []
    params, callbacks, map_err = out_port_map(outs_decl, gcase, audio_obj=True)
    n_prints = 0
    if params or callbacks:
        n_prints = max(k for k, _ in params + callbacks) + 1
    patch, out_paths = build_driver(c_name, gcase, frames, tmp, n_prints)
    if patch is None:
        return ("no-capturable-output", map_err or "")
    for p in out_paths:
        if os.path.exists(p):
            os.remove(p)

    proc, err = launch_pd(pd_exe, externals, patch, tmp, timeout)
    if err:
        return ("error", err)

    results = []
    if gout:
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
            results.append(("error", "no-output-captured"))
        else:
            produced = [{"name": f"c{i}", "samples": s}
                        for i, s in enumerate(out) if s is not None]
            results.append(compare_audio(produced, gout, atol, rtol))
    if n_prints:
        caps = parse_prints(proc)
        results += compare_captured_controls(caps, params, callbacks, gcase,
                                             atol, rtol)
    elif map_err:
        results.append((map_err, ""))
    return fold_results(results)


def run_control_case(pd_exe, externals, c_name, gcase, tmp, timeout,
                     outs_decl, atol, rtol):
    """One golden case of a control/message object: set controls, replay
    messages, bang, capture outlets via [print]. Returns (verdict, detail)."""
    params, callbacks, map_err = out_port_map(outs_decl, gcase, audio_obj=False)
    n_outlets = len(outs_decl) if outs_decl is not None else \
        max([k + 1 for k, _ in params + callbacks] or [0])
    patch = build_control_driver(c_name, gcase, n_outlets)
    proc, err = launch_pd(pd_exe, externals, patch, tmp, timeout)
    if err:
        return ("error", err) if "timeout" in err else (err, "")

    if not (params or callbacks):
        # Nothing capturable (midi/texture/geometry-only outputs, or none):
        # the object still instantiated and survived the sends + bang.
        gtex = gcase.get("outputs", {}).get("texture") or []
        if map_err:
            return (map_err, "")
        return ("no-pd-representation" if gtex else "instantiated", "")
    caps = parse_prints(proc)
    results = compare_captured_controls(caps, params, callbacks, gcase,
                                        atol, rtol)
    return fold_results(results)


def fold_results(results):
    """Combine several (verdict, detail) comparisons of one case: any MISMATCH
    or error wins, else match if anything matched."""
    if not results:
        return ("no-capturable-output", "")
    for bad in ("MISMATCH", "error"):
        for v, d in results:
            if v == bad:
                return (v, d)
    for v, d in results:
        if v == "match":
            return ("match", "; ".join(x[1] for x in results if x[0] == "match"))
    return results[0]


def run_object(pd_exe, externals, g, atol, rtol, tmp, timeout, dumps_dir):
    c_name = g["c_name"]
    entry = {"name": c_name, "cases": []}
    if not os.path.exists(os.path.join(externals, c_name + ".dll")):
        entry["verdict"] = "no-external"
        return entry
    outs_decl = load_dump(dumps_dir, g.get("_stem"))

    gcases = g.get("cases")
    if gcases is None:  # old single-case schema
        gcases = [{"inputs": g.get("inputs", {}),
                   "outputs": g.get("outputs", {})}]
    verdicts = []
    for ci, gcase in enumerate(gcases):
        rec = {"index": ci}
        gout_a = gcase.get("outputs", {}).get("audio") or []
        gin_a = gcase.get("inputs", {}).get("audio") or []
        if gout_a or gin_a:
            v, d = run_audio_case(pd_exe, externals, c_name, gcase,
                                  g.get("meta", {}), tmp, timeout, outs_decl,
                                  atol, rtol)
        else:
            v, d = run_control_case(pd_exe, externals, c_name, gcase, tmp,
                                    timeout, outs_decl, atol, rtol)
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
                # file stem = CMake target name = the introspection dump's name
                g["_stem"] = os.path.splitext(os.path.basename(f))[0]
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
    ap.add_argument("--dumps", default=None,
                    help="introspection dump JSON dir (declaration-ordered "
                         "outlet mapping); default: goldens dir with 'golden' "
                         "replaced by 'dump'")
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

    dumps_dir = args.dumps
    if dumps_dir is None:
        cand = os.path.abspath(args.goldens).replace("golden", "dump")
        dumps_dir = cand if os.path.isdir(cand) else None

    goldens = load_goldens(args.goldens)
    if args.only:
        goldens = [g for g in goldens if g["c_name"] == args.only]

    counts, mism, report = {}, [], []
    for g in goldens:
        entry = run_object(args.pd, args.externals, g, args.atol, args.rtol,
                           tmp, args.timeout, dumps_dir)
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
