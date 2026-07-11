#!/usr/bin/env python3
r"""Golden differential testing for the GStreamer binding (headless).

For every golden JSON (produced by the `golden` backend), if the object built a
GStreamer element (element name == c_name), replay each recorded case through a
one-shot gst-launch pipeline:

  filesrc(golden input, raw F32LE interleaved) -> rawaudioparse -> audioconvert
    -> <element  prop=value ...> -> filesink(out, raw F32LE interleaved)

then de-interleave the output and diff it against the golden's recorded output
audio. Input controls are set as GObject properties (the binding names them by
the lowercased port symbol). Generators (no input audio) are fed silence of the
golden's output frame count so they cook.

GStreamer is an audio/video streaming framework: control-only objects (no audio
ports) and non-audio outputs have no headless read-back path through gst-launch,
so they are reported skipped, not failed -- the same graceful gap Pd has for
shapes it can't represent.

The GStreamer binding only compiles under clang64/MSYS2, so point --gst-bin at
the MSYS2 clang64 bin dir and --plugins at the built plugin dir. The diff runs
under any Python with numpy.

Example:

  python run_gst_golden.py ^
    --gst-bin D:/msys64/clang64/bin ^
    --plugins D:/build/build-gst-clang64/gstreamer ^
    --goldens D:/build/build-avendish-msvc/golden/Debug
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from golden_compare import (aggregate_case_verdicts, compare_audio,
                            compare_textures)

import numpy as np


def gst_env(gst_bin, plugins):
    env = dict(os.environ)
    env["PATH"] = gst_bin + os.pathsep + env.get("PATH", "")
    env["GST_PLUGIN_PATH"] = plugins
    # Keep the registry out of the user's home so a stale cache can't hide a
    # freshly-built plugin.
    env["GST_REGISTRY"] = os.path.join(plugins, ".avnd_gst_registry.bin")
    return env


def norm(name):
    """Match a golden control name to a gst property: the binding lowercases the
    port symbol and turns every non-alnum char into '-' (canonical_name), so
    compare on lowercase-alnum-only ('Combo_A' -> 'comboa' == 'combo-a')."""
    return re.sub(r"[^a-z0-9]", "", str(name).lower())


def inspect_element(gst_bin, env, element):
    """Return (properties, has_sink, media) for `element`, or (None, False, None)
    if it does not exist. `has_sink` is False for a pure source (generator);
    `media` is 'audio' or 'video' (from the pad caps). Parses
    `gst-inspect-1.0 <element>`."""
    exe = os.path.join(gst_bin, "gst-inspect-1.0.exe")
    try:
        p = subprocess.run([exe, element], capture_output=True, text=True,
                           env=env, timeout=30)
    except Exception:
        return (None, False, None)
    if p.returncode != 0:
        return (None, False, None)
    props, in_props = set(), False
    has_sink = False
    media = None
    for line in p.stdout.splitlines():
        if re.match(r"^\s+SINK template:", line):
            has_sink = True
        if "video/x-raw" in line:
            media = "video"
        elif "audio/x-raw" in line and media is None:
            media = "audio"
        if line.startswith("Element Properties"):
            in_props = True
            continue
        if not in_props:
            continue
        m = re.match(r"^  (\w[\w-]*)\s+:", line)
        if m:
            props.add(m.group(1))
    props -= {"name", "parent", "qos"}
    return (props, has_sink, media)


def prop_arg(name, value):
    """Format one `name=value` token for gst-launch, or None to skip."""
    if isinstance(value, bool):
        return f"{name}={'true' if value else 'false'}"
    if isinstance(value, (int, float)):
        return f"{name}={value}"
    if isinstance(value, str):
        if value == "unrecorded":
            return None
        return f'{name}="{value}"'
    return None


def interleave(channels):
    """[[ch0..],[ch1..]] -> interleaved little-endian float32 bytes."""
    arr = np.asarray(channels, dtype="<f4")
    if arr.ndim == 1:
        arr = arr.reshape(1, -1)
    return arr.T.copy().tobytes()


def deinterleave(raw, nch):
    """Interleaved F32LE bytes -> [[ch0..],[ch1..]] (nch channels)."""
    flat = np.frombuffer(raw, dtype="<f4")
    if nch <= 0:
        return []
    usable = (len(flat) // nch) * nch
    if usable == 0:
        return []
    frames = flat[:usable].reshape(-1, nch)
    return [frames[:, c].tolist() for c in range(nch)]


def build_prop_tokens(props, gcase):
    """gst-launch `name=value` tokens for the golden controls the element
    actually exposes (matched by normalized name)."""
    by_norm = {norm(p): p for p in props}
    tokens = []
    for c in gcase.get("inputs", {}).get("controls", []):
        real = by_norm.get(norm(c.get("name", "")))
        if real is None:
            continue
        tok = prop_arg(real, c.get("value"))
        if tok:
            tokens.append(tok)
    return tokens


def parse_caps_dims(text):
    """Pull the (width, height) of the negotiated fakesink caps out of
    gst-launch -v output; fall back to the last width/height pair seen."""
    fakesink_dims, last_dims = None, None
    for line in text.splitlines():
        m = re.search(r"width=\(int\)(\d+).*?height=\(int\)(\d+)", line)
        if not m:
            continue
        last_dims = (int(m.group(1)), int(m.group(2)))
        if "fakesink" in line.lower():
            fakesink_dims = last_dims
    return fakesink_dims or last_dims


def run_texture_case(exe, env, element, props, has_sink, gcase, tmp):
    """Run one texture case; return ([{width,height}], error). Reads the output
    resolution from the negotiated caps (gst-launch -v). Content bytes are not
    captured -- a host's readback format isn't the golden's native layout, so
    dimensions are the authoritative cross-backend check."""
    prop_tokens = build_prop_tokens(props, gcase)
    if not has_sink:
        # Generator (Source/Video): output size is the element's own.
        pipeline = [exe, "-v", element, *prop_tokens, "num-buffers=1", "!",
                    "fakesink"]
    else:
        # Filter: feed a 16x16 RGBA frame -- the size the golden synthesizes for
        # texture inputs -- so a size-preserving filter yields 16x16 and a
        # resampler yields its own size.
        pipeline = [exe, "-v",
                    "videotestsrc", "num-buffers=1", "!",
                    "video/x-raw,format=RGBA,width=16,height=16", "!",
                    "videoconvert", "!",
                    element, *prop_tokens, "!", "fakesink"]
    try:
        p = subprocess.run(pipeline, capture_output=True, text=True, env=env,
                           timeout=60)
    except subprocess.TimeoutExpired:
        return (None, "timeout")
    dims = parse_caps_dims((p.stdout or "") + "\n" + (p.stderr or ""))
    if dims is None:
        err = (p.stderr or p.stdout or "").strip().splitlines()
        return (None, "no-caps: " + (err[-1] if err else "?"))
    return ([{"width": dims[0], "height": dims[1], "hash": None}], None)


def run_case(exe, env, element, props, has_sink, gcase, meta, tmp):
    """Run one golden AUDIO case through gst-launch; return (out_channels, error)."""
    gin = gcase.get("inputs", {}).get("audio") or []
    gout = gcase.get("outputs", {}).get("audio") or []
    out_nch = len(gout)
    rate = int(meta.get("rate", 44100))
    # Build paths with forward slashes: a backslash from os.path.join is eaten by
    # gst-launch's argument parser (location=..\gst_in.raw -> gst_in.raw lost).
    base = tmp.rstrip("/\\").replace("\\", "/")
    out_path = base + "/gst_out.raw"
    in_path = base + "/gst_in.raw"
    if os.path.exists(out_path):
        os.remove(out_path)

    prop_tokens = build_prop_tokens(props, gcase)

    if not has_sink:
        # Pure source (generator): no input pad. Produce one block and capture.
        out_frames = len(gout[0]) if (gout and gout[0]) else int(meta.get("frames", 64))
        pipeline = [
            exe, "-q",
            element, *prop_tokens, "num-buffers=1", "!",
            "filesink", f"location={out_path}",
        ]
    else:
        if gin:
            in_nch = len(gin)
            in_frames = len(gin[0]) if gin[0] else 0
            raw = interleave(gin)
        else:
            # A filter with no recorded input audio: feed silence of the golden
            # output length so it cooks that many frames.
            in_nch = 1
            in_frames = len(gout[0]) if (gout and gout[0]) else int(meta.get("frames", 64))
            raw = np.zeros(in_frames * in_nch, dtype="<f4").tobytes()
        if in_frames == 0:
            return (None, "empty-input")
        with open(in_path, "wb") as f:
            f.write(raw)
        pipeline = [
            exe, "-q",
            "filesrc", f"location={in_path}", "!",
            "rawaudioparse", "use-sink-caps=false", "format=pcm",
            "pcm-format=f32le", f"sample-rate={rate}", f"num-channels={in_nch}", "!",
            "audioconvert", "!",
            element, *prop_tokens, "!",
            "filesink", f"location={out_path}",
        ]
    try:
        p = subprocess.run(pipeline, capture_output=True, text=True, env=env,
                           timeout=60)
    except subprocess.TimeoutExpired:
        return (None, "timeout")
    if not os.path.exists(out_path):
        err = (p.stderr or p.stdout or "").strip().splitlines()
        return (None, "pipeline-fail: " + (err[-1] if err else "?"))
    with open(out_path, "rb") as f:
        raw_out = f.read()
    return (deinterleave(raw_out, out_nch or in_nch), None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gst-bin", required=True, help="MSYS2 clang64 bin dir")
    ap.add_argument("--plugins", required=True, help="built gstreamer plugin dir")
    ap.add_argument("--goldens", required=True, help="golden JSON dir")
    ap.add_argument("--atol", type=float, default=1e-3, help="abs tolerance")
    ap.add_argument("--rtol", type=float, default=1e-3, help="rel tolerance")
    ap.add_argument("--report", default="gst_golden_report.json")
    ap.add_argument("--only", help="run a single object (c_name)")
    ap.add_argument("--tmp", default=os.environ.get("TEMP", "."),
                    help="scratch dir for raw PCM files")
    args = ap.parse_args()

    env = gst_env(args.gst_bin, args.plugins)
    exe = os.path.join(args.gst_bin, "gst-launch-1.0.exe")

    goldens = []
    for f in sorted(glob.glob(os.path.join(args.goldens, "*.json"))):
        try:
            g = json.load(open(f))
            if g.get("c_name"):
                goldens.append(g)
        except Exception:
            pass

    counts, mism, report = {}, [], []
    for g in goldens:
        c_name = g["c_name"]
        if args.only and c_name != args.only:
            continue
        entry = {"name": c_name, "cases": []}
        report.append(entry)

        props, has_sink, media = inspect_element(args.gst_bin, env, c_name)
        if props is None:
            entry["verdict"] = "no-element"
            counts["no-element"] = counts.get("no-element", 0) + 1
            continue

        gcases = g.get("cases")
        if gcases is None:
            gcases = [{"inputs": g.get("inputs", {}),
                       "outputs": g.get("outputs", {})}]
        verdicts = []
        for ci, gcase in enumerate(gcases):
            # Impulse-engagement / message-invocation cases have no gst-launch
            # driving mechanism; skip them honestly.
            if gcase.get("kind") in ("impulse", "message"):
                verdicts.append((ci, "unsupported-case-kind", gcase["kind"]))
                continue
            gaud = gcase.get("outputs", {}).get("audio") or []
            gtex = gcase.get("outputs", {}).get("texture") or []
            rec = {"index": ci}
            if media == "video" and gtex:
                tex, err = run_texture_case(exe, env, c_name, props, has_sink,
                                            gcase, args.tmp)
                if err:
                    verdicts.append((ci, "error", err))
                    rec["verdict"], rec["error"] = "error", err
                else:
                    # TD/gst hand textures back converted, so dimensions are
                    # authoritative and the byte hash is informational.
                    v, d = compare_textures(tex, gtex, args.atol, args.rtol,
                                            content="dims")
                    verdicts.append((ci, v, d))
                    rec["verdict"], rec["detail"], rec["tex"] = v, d, tex
            elif gaud:
                out_ch, err = run_case(exe, env, c_name, props, has_sink, gcase,
                                       g.get("meta", {}), args.tmp)
                if err:
                    verdicts.append((ci, "error", err))
                    rec["verdict"], rec["error"] = "error", err
                else:
                    v, d = compare_audio(
                        [{"name": f"c{i}", "samples": s}
                         for i, s in enumerate(out_ch)], gaud, args.atol, args.rtol)
                    verdicts.append((ci, v, d))
                    rec["verdict"], rec["detail"] = v, d
            else:
                # No audio/texture output to diff -- gst has no headless
                # control read-back through gst-launch.
                verdicts.append((ci, "no-audio-io", ""))
                rec["verdict"] = "no-audio-io"
            entry["cases"].append(rec)

        v, detail = aggregate_case_verdicts(verdicts)
        entry["verdict"] = v
        counts[v] = counts.get(v, 0) + 1
        if v in ("MISMATCH", "error"):
            mism.append(f"  {v} {c_name} ({len(verdicts)} cases): {detail[:140]}")
        json.dump(report, open(args.report, "w"), indent=1)

    ncases = sum(len(e.get("cases", [])) for e in report)
    print(f"\n=== gstreamer diff vs golden ({len(report)} objects, "
          f"{ncases} cases) ===")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")
    for m in mism:
        print(m)
    print(f"report -> {args.report}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
