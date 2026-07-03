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
        if gv == "unrecorded" or not isinstance(gv, (int, float, str)):
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
        if isinstance(tv, bool):
            tv = float(tv)
        if not isinstance(tv, (int, float)):
            continue
        checked += 1
        if abs(tv - gv) > maxdiff:
            maxdiff, worst = abs(tv - gv), nm
    if checked == 0:
        return ("no-name-match", f"got={list(named)[:4]}")
    peak = max((abs(gc["value"]) for gc in golden
                if isinstance(gc.get("value"), (int, float))), default=0.0)
    tol = atol + rtol * peak
    return ("match" if maxdiff <= tol else "MISMATCH",
            f"{checked} ctrls maxdiff={maxdiff:.2e}@{worst} tol={tol:.2e}")


def compare_textures(textures, golden, atol, rtol):
    """Diff captured texture outputs against the golden's recorded
    {width, height, hash} (FNV-1a over the raw pixel bytes -- see fnv1a64).
    `textures` is [{width, height, hash}] in port order."""
    if not golden:
        return ("no-golden-texture", "")
    if not textures:
        return ("no-backend-texture", "")
    n = min(len(textures), len(golden))
    for i in range(n):
        t, g = textures[i], golden[i]
        if (t.get("width"), t.get("height")) != (g.get("width"), g.get("height")):
            return ("MISMATCH",
                    f"tex{i} size {t.get('width')}x{t.get('height')} "
                    f"vs gold {g.get('width')}x{g.get('height')}")
        if t.get("hash") != g.get("hash"):
            return ("MISMATCH", f"tex{i} content hash differs")
    return ("match", f"{n} textures")


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
