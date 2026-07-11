"""Shared golden-differential comparison helpers.

Every backend harness (TouchDesigner sweep, Python driver, ...) reads a golden
JSON produced by the `golden` backend, replays its recorded inputs, captures the
backend's outputs and diffs them with these functions, so tolerance and
name-matching semantics stay identical across backends.
"""


def compare_audio(out, golden, atol, rtol):
    """Diff captured output channels (positional) against the golden audio
    channels over their overlapping prefix (a backend's cook sample-count can
    differ from the golden's frame count). `out` is [{name, samples}]."""
    if not golden:
        return ("no-golden-audio", "")
    if not out:
        return ("no-backend-audio", "")
    ch = [e["samples"] for e in out]
    nch = min(len(ch), len(golden))
    if nch == 0:
        return ("empty", "")
    maxdiff = 0.0
    for c in range(nch):
        ns = min(len(ch[c]), len(golden[c]))
        for i in range(ns):
            maxdiff = max(maxdiff, abs(ch[c][i] - golden[c][i]))
    peak = max((abs(x) for gch in golden for x in gch), default=0.0)
    tol = atol + rtol * peak
    verdict = "match" if maxdiff <= tol else "MISMATCH"
    detail = (f"maxdiff={maxdiff:.2e} tol={tol:.2e} "
              f"got={len(ch)}ch gold={len(golden)}ch")
    return (verdict, detail)


def compare_controls(named, golden, atol, rtol):
    """Diff golden output controls against backend values matched by name.
    `named` is {name -> value}; exact, lowercase and capitalized forms of the
    golden name are tried."""
    if not golden:
        return ("no-golden-controls", "")
    if not named:
        return ("no-backend-controls", "")
    checked, maxdiff, worst = 0, 0.0, ""
    for gc in golden:
        if not isinstance(gc, dict):
            continue
        nm, gv = gc.get("name", ""), gc.get("value")
        if gv == "unrecorded" or not isinstance(gv, (int, float, str, list)):
            continue
        tv = None
        for k in (nm, nm.lower(), nm.capitalize()):
            if k in named:
                tv = named[k]
                break
        if tv is None:
            continue
        if isinstance(gv, str):
            # String outputs: exact equality.
            checked += 1
            if str(tv) != gv and maxdiff == 0.0:
                maxdiff, worst = 1.0, f"{nm}({tv!r}!={gv!r})"
            continue
        if isinstance(gv, list):
            # Container outputs (vector / array / aggregate): elementwise.
            tvl = tv if isinstance(tv, list) else [tv]
            checked += 1
            if len(tvl) != len(gv):
                if maxdiff == 0.0:
                    maxdiff, worst = float("inf"), f"{nm}(len {len(tvl)}!={len(gv)})"
                continue
            for a, b in zip(tvl, gv):
                if isinstance(a, bool):
                    a = float(a)
                if isinstance(b, bool):
                    b = float(b)
                if isinstance(a, (int, float)) and isinstance(b, (int, float)):
                    if abs(a - b) > maxdiff:
                        maxdiff, worst = abs(a - b), nm
                elif str(a) != str(b) and maxdiff == 0.0:
                    maxdiff, worst = 1.0, f"{nm}({a!r}!={b!r})"
            continue
        if isinstance(tv, bool):
            tv = float(tv)
        if not isinstance(tv, (int, float)):
            continue
        checked += 1
        if abs(tv - gv) > maxdiff:
            maxdiff, worst = abs(tv - gv), nm
    if checked == 0:
        return ("no-name-match", f"got={list(named)[:4]}")
    peak = 0.0
    for gc in golden:
        v = gc.get("value")
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            peak = max(peak, abs(v))
        elif isinstance(v, list):
            for x in v:
                if isinstance(x, (int, float)) and not isinstance(x, bool):
                    peak = max(peak, abs(x))
    tol = atol + rtol * peak
    return ("match" if maxdiff <= tol else "MISMATCH",
            f"{checked} ctrls maxdiff={maxdiff:.2e}@{worst} tol={tol:.2e}")


def compare_textures(textures, golden, atol, rtol, content="hash"):
    """Diff captured texture outputs against the golden's recorded
    {width, height, hash}. `textures` is [{width, height, hash}] in port order.

    content="hash" (default): dimensions AND the FNV-1a byte hash must match --
    valid only when the backend reads pixels back in the object's NATIVE byte
    layout (the golden hashes native bytes; the Python binding does too).
    content="dims": dimensions are authoritative, the hash is informational --
    for hosts (TouchDesigner, GStreamer) that hand back a normalized/converted
    representation whose bytes never equal the native golden hash. Wrong output
    SIZE is still a real failure and is caught."""
    if not golden:
        return ("no-golden-texture", "")
    if not textures:
        return ("no-backend-texture", "")
    n = min(len(textures), len(golden))
    hashes_match = True
    for i in range(n):
        t, g = textures[i], golden[i]
        if (t.get("width"), t.get("height")) != (g.get("width"), g.get("height")):
            return ("MISMATCH",
                    f"tex{i} size {t.get('width')}x{t.get('height')} "
                    f"vs gold {g.get('width')}x{g.get('height')}")
        if t.get("hash") != g.get("hash"):
            hashes_match = False
            if content == "hash":
                return ("MISMATCH", f"tex{i} content hash differs")
    if content == "dims":
        note = "content byte-identical" if hashes_match else "dims only"
        return ("match", f"{n} textures ({note})")
    return ("match", f"{n} textures")


def compare_callbacks(events, golden_events, atol, rtol):
    """Diff one callback port's captured firings against the golden's recorded
    events. Both are lists of argument lists; the event count and each
    argument must match (numeric within tolerance, strings exact, positions the
    golden recorded as 'unrecorded' skipped)."""
    if golden_events is None:
        return ("no-golden-callbacks", "")
    if len(events) != len(golden_events):
        return ("MISMATCH",
                f"{len(events)} events vs gold {len(golden_events)}")
    for i, (e, g) in enumerate(zip(events, golden_events)):
        # An 'unrecorded' golden argument is a container/aggregate whose
        # backend representation may expand to several atoms: give up on
        # arity for that event and only compare the positions before it.
        relaxed = "unrecorded" in g
        if not relaxed and len(e) != len(g):
            return ("MISMATCH", f"event{i} arity {len(e)} vs {len(g)}")
        for j, (a, b) in enumerate(zip(e, g)):
            if b == "unrecorded":
                if relaxed:
                    break
                continue
            if isinstance(b, bool):
                b = float(b)
            if isinstance(a, (int, float)) and isinstance(b, (int, float)):
                if abs(a - b) > atol + rtol * abs(b):
                    return ("MISMATCH", f"event{i}[{j}] {a} vs {b}")
            elif str(a) != str(b):
                return ("MISMATCH", f"event{i}[{j}] {a!r} vs {b!r}")
    return ("match", f"{len(events)} events")


def fnv1a64(data):
    """FNV-1a 64-bit over raw bytes, returned as a SIGNED int64 -- the exact
    hash the golden generator records (binding/golden/run.hpp)."""
    h = 1469598103934665603
    for b in data:
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h - 0x10000000000000000 if h >= 0x8000000000000000 else h


def aggregate_case_verdicts(verdicts):
    """Fold per-case (index, verdict, detail) tuples into one per-object
    verdict: any MISMATCH wins (reporting the first failing case), else match
    if any case matched, else the first non-match category."""
    mm = [x for x in verdicts if x[1] == "MISMATCH"]
    if mm:
        detail = f"case{mm[0][0]} {mm[0][2]}"
        if len(mm) > 1:
            detail += f" (+{len(mm)-1} more cases)"
        return ("MISMATCH", detail)
    if any(x[1] == "match" for x in verdicts):
        return ("match", "")
    if verdicts:
        return (verdicts[0][1], verdicts[0][2])
    return ("no-golden-output", "")
