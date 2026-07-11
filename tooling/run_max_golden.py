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
                        real value diff; string controls, impulse engagements
                        ([<PortName>( selector) and declared message-port
                        invocations are replayed; callback firings are captured
                        as ordered event lists and diffed per event.
  * audio objects    -> MC signal outlet captured with record~ (real sample diff
                        when DSP runs); control/callback outlets (to the right
                        of the signal outlet) captured after a post-block bang,
                        which the audio binding answers by committing its
                        non-audio outputs (analyzers like avnd_peak).
  * texture objects  -> jit.matrix MOP: filters are fed a 16x16 char matrix,
                        generators banged; the output matrix dims are read back
                        via a named [jit.matrix @adapt 1] and diffed in "dims"
                        mode (Jitter converts content, so bytes are
                        informational only).
  * none             -> nothing feasible to capture: instantiate-only smoke
                        (verdict 'instantiated'), never a false pass.
Outlet -> port mapping comes from the introspection dumps (--dumps, declaration
order); without a dump, positional mapping applies when the object has only one
kind of capturable output.

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
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from golden_compare import (aggregate_case_verdicts, compare_audio,
                            compare_callbacks, compare_controls,
                            compare_textures)

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


def max_symbol(name):
    """The runtime selector the Max binding matches a port/message name
    against: avnd::get_static_symbol / fixup_identifier REPLACES every char
    outside [A-Za-z0-9.~] with '_' (see binding/max/helpers.hpp
    valid_char_for_name + common/metadatas.hpp fixup_identifier)."""
    return re.sub(r"[^A-Za-z0-9.~]", "_", str(name))


def load_dump_outputs(dumps_dir, stem):
    """Declaration-ordered output ports [{name, type}] from the introspection
    dump JSON for this object (types: parameter/callback/audio/texture/...),
    or None if unavailable."""
    if not dumps_dir or not stem:
        return None
    p = os.path.join(dumps_dir, stem + ".json")
    if not os.path.exists(p):
        return None
    try:
        outs = json.load(open(p, encoding="utf-8")).get("outputs")
        return outs if isinstance(outs, list) else []
    except Exception:
        return None


def cap_layout(outs_decl, gcases, kind):
    """(capBase, caps) -- which object outlets carry capturable messages.
    caps is [[type, name], ...] in outlet order starting at outlet capBase.
    For message objects every output port gets an outlet (declaration order,
    capBase 0); for audio objects the non-audio ports sit to the RIGHT of the
    single mc signal outlet (capBase 1 -- or 0 when the object has no
    audio-output port, e.g. analyzers)."""
    if outs_decl is None:
        # No dump: positional fallback from the golden's recorded outputs
        # (valid when the object has only one kind of capturable output).
        gc = (gcases[0].get("outputs", {}).get("controls") or [])
        gcb = (gcases[0].get("outputs", {}).get("callbacks") or [])
        if gc and gcb:
            return (0, None)  # ambiguous
        caps = [["parameter", c.get("name", f"p{i}")] for i, c in enumerate(gc)]
        caps += [["callback", c.get("name", f"p{i}")] for i, c in enumerate(gcb)]
        return (0, caps)
    has_audio_out = any(o.get("type") == "audio" for o in outs_decl)
    caps, pi, cbi = [], 0, 0
    for o in outs_decl:
        ty = o.get("type", "")
        if kind == "audio" and ty == "audio":
            continue  # folded into the single mc signal outlet
        nm = o.get("name")
        if ty == "parameter":
            caps.append([ty, nm if nm else f"p{pi}"])
            pi += 1
        elif ty == "callback":
            caps.append([ty, nm if nm else f"p{cbi}"])
            cbi += 1
        else:
            caps.append([ty, nm or ""])  # midi/texture/...: occupies an outlet
    base = 1 if (kind == "audio" and has_audio_out) else 0
    return (base, caps)


def gen_data_js(goldens, out_path, dumps_dir):
    """Emit avnd_max_data.js: `var AVND_DATA = [...]` -- the in-Max driver
    include()s it, so it never has to parse JSON. One entry per object with its
    kind, outlet-capture layout, and per case the input controls (numeric,
    string), impulse engagements, message invocations, audio and texture."""
    objs = []
    for g in goldens:
        cases_out = []
        cases = g.get("cases") or [{"inputs": g.get("inputs", {}),
                                    "outputs": g.get("outputs", {})}]
        kind = "none"
        for c in cases:
            ins, outs = c.get("inputs", {}), c.get("outputs", {})
            controls, impulses = [], []
            for k, cc in enumerate(ins.get("controls", [])):
                v = cc.get("value")
                nm = max_symbol(cc.get("name", ""))  # runtime selector form
                if v == "unrecorded" or v is None:
                    continue
                if v == "bang":
                    impulses.append(nm)
                elif isinstance(v, bool):
                    controls.append([cc.get("index", k), nm, int(v)])
                elif isinstance(v, list):
                    # container control (xy / rgba / array / list): feed all
                    # components as one list message to the inlet.
                    if all(isinstance(x, (bool, int, float, str)) for x in v):
                        controls.append([cc.get("index", k), nm,
                                         [int(x) if isinstance(x, bool) else x
                                          for x in v]])
                elif isinstance(v, (int, float, str)):
                    controls.append([cc.get("index", k), nm, v])
            messages = [[max_symbol(m.get("name", "")), m.get("args", [])]
                        for m in (ins.get("messages") or [])
                        if m.get("status") == "invoked"]
            audio_in = ins.get("audio") or []
            tex_in = [[t.get("width", 16), t.get("height", 16)]
                      for t in (ins.get("texture") or [])]
            has_aout = bool(outs.get("audio"))
            has_ain = bool(audio_in)
            has_cout = bool(outs.get("controls") or outs.get("callbacks"))
            has_tex = bool(outs.get("texture"))
            if has_aout or has_ain:
                kind = "audio"
            elif has_tex and kind != "audio":
                kind = "texture"
            elif has_cout and kind not in ("audio", "texture"):
                kind = "control"
            cases_out.append({
                "controls": controls,
                "impulses": impulses,
                "messages": messages,
                "audioIn": audio_in,
                "texIn": tex_in,
                "nAudioOut": len(outs.get("audio") or []),
                "nTexOut": len(outs.get("texture") or []),
            })
        outs_decl = load_dump_outputs(dumps_dir, g.get("_stem"))
        base, caps = cap_layout(outs_decl, cases, kind)
        objs.append({"name": g["c_name"], "kind": kind, "capBase": base,
                     "caps": caps if caps is not None else [],
                     "capsAmbiguous": caps is None, "cases": cases_out})
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
                # file stem = CMake target name = the introspection dump's name
                g["_stem"] = os.path.splitext(os.path.basename(f))[0]
                out.append(g)
        except Exception:
            pass
    return out


def parse_report(path):
    """Report is JSON-lines (one object per line), written by the js driver and
    appended-to by us on crash. Returns {name: entry}.

    Max's File('write') flushes its internal buffer every 32 KB with a raw
    newline, so a long object line (audio arrays) can be split across several
    physical lines. Rejoin greedily: accumulate lines until the buffer parses
    as one object, then reset. A stray unparseable fragment is dropped when the
    next '{' starts, so a single corrupt object can't swallow the rest."""
    entries = {}
    if not os.path.exists(path):
        return entries
    buf = ""
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line.strip() and not buf:
            continue
        # A new object starts here and the buffer never parsed -> drop it.
        if buf and line.lstrip().startswith("{"):
            try:
                e = json.loads(buf)
                entries[e["name"]] = e
            except Exception:
                pass
            buf = ""
        buf += line
        try:
            e = json.loads(buf)
            entries[e["name"]] = e
            buf = ""
        except Exception:
            pass
    if buf:
        try:
            e = json.loads(buf)
            entries[e["name"]] = e
        except Exception:
            pass
    return entries


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max", required=True, help="Max.exe")
    ap.add_argument("--externals", required=True, help="dir of built *.mxe64")
    ap.add_argument("--goldens", required=True, help="golden JSON dir")
    ap.add_argument("--dumps", default=None,
                    help="introspection dump JSON dir (declaration-ordered "
                         "outlet mapping); default: goldens dir with 'golden' "
                         "replaced by 'dump'")
    ap.add_argument("--atol", type=float, default=1e-3)
    ap.add_argument("--rtol", type=float, default=1e-3)
    ap.add_argument("--timeout", type=int, default=240,
                    help="seconds per launch before giving up")
    ap.add_argument("--max-launches", type=int, default=80)
    ap.add_argument("--only", help="comma-separated c_names to restrict to")
    ap.add_argument("--recompute", action="store_true",
                    help="skip driving Max; recompute verdicts from an existing "
                         "report (useful to re-diff after a compare-logic change)")
    ap.add_argument("--report", default=None,
                    help="report path (default: <externals>/avnd_max_report.jsonl)")
    args = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    asset_dir = os.path.join(here, "max")
    ext_dir = os.path.abspath(args.externals).replace("\\", "/")

    dumps_dir = args.dumps
    if dumps_dir is None:
        cand = os.path.abspath(args.goldens).replace("golden", "dump")
        dumps_dir = cand if os.path.isdir(cand) else None

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
    data_objs = gen_data_js(goldens, data_js, dumps_dir)
    datamap = {o["name"]: o for o in data_objs}
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

    if not args.recompute:
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

    dismissed, crashers = 0, []
    if not args.recompute:
        subprocess.run(kill, capture_output=True)
    for attempt in range(1, (0 if args.recompute else args.max_launches) + 1):
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
    def fold(results):
        """Combine several (verdict, detail) comparisons of one case."""
        if not results:
            return ("no-capturable-output", "")
        for bad in ("MISMATCH", "error"):
            for v, d in results:
                if v == bad:
                    return (v, d)
        for v, d in results:
            if v == "match":
                return ("match",
                        "; ".join(x[1] for x in results if x[0] == "match"))
        return results[0]

    def caps_compare(rc, gc, dobj):
        """Diff a case's captured outlet events against the golden's output
        controls and callback events. rc['caps'] = {capIdx(str): [event,...]},
        each event a list of atoms ('bang' -> valueless firing)."""
        results = []
        gout = gc.get("outputs", {})
        gctl = gout.get("controls") or []
        gcb = gout.get("callbacks") or []
        recordable = [c for c in gctl if isinstance(c, dict)
                      and c.get("value") != "unrecorded"]
        if dobj.get("capsAmbiguous"):
            return [("ambiguous-outlets", "params+callbacks but no dump")]
        caps = dobj.get("caps") or []
        rcaps = rc.get("caps") or {}

        def events(i):
            evs = rcaps.get(str(i)) or []
            return [[] if e == ["bang"] else e for e in evs]
        if recordable:
            named = {}
            for i, (ty, nm) in enumerate(caps):
                if ty != "parameter":
                    continue
                evs = events(i)
                if evs and evs[-1]:
                    ev = evs[-1]
                    named[nm] = ev[0] if len(ev) == 1 else ev
            results.append(compare_controls(named, gctl, args.atol, args.rtol))
        cbi = 0
        for i, (ty, nm) in enumerate(caps):
            if ty != "callback":
                continue
            gev = None
            for cb in gcb:
                if cb.get("name") == nm or cb.get("index") == cbi:
                    gev = cb.get("events")
                    break
            cbi += 1
            if gev is None:
                continue
            v, d = compare_callbacks(events(i), gev, args.atol, args.rtol)
            results.append((v, f"cb {nm}: {d}"))
        return results

    entries = parse_report(report)
    counts, details, rows = {}, [], []
    for g in goldens:
        name = g["c_name"]
        e = entries.get(name)
        dobj = datamap.get(name, {})
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
                results = []
                if gout.get("audio") or gc.get("inputs", {}).get("audio"):
                    if rc.get("dsp") is False:
                        # Max rendered no audio (no live DSP driver in this
                        # session): the external instantiated but we can't do a
                        # sample diff -- do NOT report a false mismatch.
                        verdicts.append((ci, "audio-dsp-unavailable",
                                         "instantiated; DSP did not run"))
                        continue
                    if gout.get("audio"):
                        cap = rc.get("audio")
                        out = ([{"name": f"c{i}", "samples": ch}
                                for i, ch in enumerate(cap)] if cap else None)
                        results.append(compare_audio(
                            out, gout["audio"], args.atol, args.rtol))
                    if gout.get("controls") or gout.get("callbacks"):
                        results += caps_compare(rc, gc, dobj)
                    verdicts.append((ci,) + fold(results))
                elif gout.get("texture"):
                    rtex = rc.get("texture")
                    if rtex:
                        verdicts.append((ci,) + compare_textures(
                            rtex, gout["texture"], args.atol, args.rtol,
                            content="dims"))
                    else:
                        verdicts.append((ci, "texture-uncaptured",
                                         "jitter matrix not captured"))
                elif gout.get("controls") or gout.get("callbacks"):
                    verdicts.append((ci,) + fold(caps_compare(rc, gc, dobj)))
                else:
                    # no output to verify: object at least instantiated + ran.
                    ok = rc.get("instantiated") or rc.get("ok") \
                        or "caps" in rc
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
