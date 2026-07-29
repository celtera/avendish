#!/usr/bin/env python3
"""Validate generated Max/MSP and Pure Data help patches.

The help patches produced by ``generate_patches`` (see
HELP_PATCH_QUALITY_PLAN.md) must be *readable*, not merely parseable: no two
boxes may overlap, no box may fall outside the saved canvas, and no comment may
be clipped by its own rectangle. This tool checks exactly that, using each
host's real font metrics, plus the structural integrity of the connections.

    py check_help_patches.py --pd  build/pd
    py check_help_patches.py --max build/max/help
    py check_help_patches.py --pd build/pd --max build/max/help --json report.json

Exit status is non-zero when any patch has a problem, so it can gate CI.
"""

import argparse
import glob
import json
import os
import re
import sys

# --------------------------------------------------------------------------
# Pure Data
# --------------------------------------------------------------------------

# Pd's fixed font sizes -> (char width, line height) in pixels. Straight from
# `font_metrics` in Pd's own tcl/pd-gui.tcl; Pd picks the real font to fit these,
# so they are what the canvas actually lays out with.
PD_FONT = {8: (5, 11), 10: (6, 13), 12: (7, 16), 16: (10, 19), 24: (14, 29),
           36: (22, 44)}

# Default character width of a box with no explicit `, f N` width flag.
PD_BOXWIDTH = 60

# A `cnv` is a background decoration: only its top-left 15x15 handle is
# selectable, so text is *meant* to sit on top of it.
PD_BACKGROUND = {"cnv"}


def pd_atoms(s):
    """Split a Pd record into atoms, honouring backslash escapes."""
    out, cur, esc = [], "", False
    for ch in s:
        if esc:
            cur += ch
            esc = False
        elif ch == "\\":
            esc = True
        elif ch in " \n\t":
            if cur:
                out.append(cur)
                cur = ""
        else:
            cur += ch
    if cur:
        out.append(cur)
    return out


def pd_records(text):
    """Yield the ';'-terminated records of a Pd file, keeping escaped ';'."""
    cur, esc = "", False
    for ch in text:
        if esc:
            cur += ch
            esc = False
        elif ch == "\\":
            cur += ch
            esc = True
        elif ch == ";":
            yield cur.strip()
            cur = ""
        else:
            cur += ch
    if cur.strip():
        yield cur.strip()


def pd_wrap(text, width):
    """Number of lines Pd renders `text` on, given a width in characters.

    Mirrors Pd's rtext wrapping: break at the last space that fits, hard-break
    a word longer than the limit, and honour embedded newlines.
    """
    if width <= 0:
        width = PD_BOXWIDTH
    n = 0
    for para in text.split("\n"):
        i, end = 0, len(para)
        if end == 0:
            n += 1
            continue
        while i < end:
            if end - i <= width:
                n += 1
                break
            brk = para.rfind(" ", i, i + width + 1)
            if brk <= i:
                brk = i + width          # a single word longer than the box
                n += 1
                i = brk
            else:
                n += 1
                i = brk + 1
    return max(1, n)


def _iem_cnv(a):
    return int(float(a[2])), int(float(a[3]))


def _iem_hsl(a):
    return int(float(a[1])) + 3, int(float(a[2])) + 3


def _iem_vsl(a):
    return int(float(a[1])) + 3, int(float(a[2])) + 3


def _iem_square(a):
    s = int(float(a[1]))
    return s + 1, s + 1


def _iem_hradio(a):
    s, n = int(float(a[1])), int(float(a[4]))
    return s * n + 1, s + 1


def _iem_vradio(a):
    s, n = int(float(a[1])), int(float(a[4]))
    return s + 1, s * n + 1


def _iem_nbx(a):
    return int(float(a[1])) * 7 + 12, int(float(a[2])) + 1


def _iem_vu(a):
    return int(float(a[1])) * 3 + 4, int(float(a[2])) + 4


PD_IEMGUI = {"cnv": _iem_cnv, "hsl": _iem_hsl, "vsl": _iem_vsl, "tgl": _iem_square,
             "bng": _iem_square, "hradio": _iem_hradio, "vradio": _iem_vradio,
             "nbx": _iem_nbx, "vu": _iem_vu}


def pd_display_text(atoms):
    """The string Pd actually draws for a run of atoms.

    Pd stores ',' and ';' as atoms of their own but prints them attached to the
    preceding word (binbuf_gettext), and wrapping is computed on that rendered
    text -- so the separating space must not be counted.
    """
    s = " ".join(atoms)
    return s.replace(" ,", ",").replace(" ;", ";")


def pd_box(kind, x, y, atoms, font):
    """Bounding box of one Pd record, as Pd itself would draw it."""
    fw, fh = PD_FONT.get(font, (7, 16))
    text = pd_display_text(atoms)
    width_flag = 0
    m = re.search(r",\s*f\s+(\d+)\s*$", text)
    if m:
        width_flag = int(m.group(1))
        text = text[: m.start()].strip()

    if kind in ("floatatom", "listbox", "symbolatom"):
        chars = int(float(atoms[0])) if atoms else 5
        return dict(kind=kind, x=x, y=y, w=chars * fw + 5, h=fh + 4, text=kind)

    if kind == "obj" and atoms and atoms[0] in PD_IEMGUI:
        try:
            w, h = PD_IEMGUI[atoms[0]](atoms)
        except (IndexError, ValueError):
            w, h = 100, 20
        return dict(kind=atoms[0], x=x, y=y, w=w, h=h, text=text)

    limit = width_flag or PD_BOXWIDTH
    lines = pd_wrap(text, limit)
    cols = min(len(text), limit) if lines == 1 else limit
    pad_w, pad_h = (6, 4) if kind == "text" else (10, 6)
    return dict(kind=kind, x=x, y=y, w=cols * fw + pad_w, h=lines * fh + pad_h,
                text=text)


class Canvas:
    """One Pd canvas: the root patch or a subpatch, each with its own geometry
    and its own connection index space."""

    def __init__(self, name, w, h, font):
        self.name = name
        self.w, self.h, self.font = w, h, font
        self.boxes = []
        self.connects = []


def pd_parse(path):
    """Parse a .pd file into every canvas it contains.

    Subpatches are real canvases with their own coordinates, and the generated
    reference section lives in one -- so they must be checked too, not skipped.
    A `#X restore` closes a subpatch and puts its box on the parent canvas.
    """
    text = open(path, encoding="utf-8", errors="replace").read()
    root = None
    stack = []
    canvases = []
    for rec in pd_records(text):
        rec = rec.replace("\n", " ").strip()
        if not rec:
            continue
        a = pd_atoms(rec)
        if len(a) < 2:
            continue
        if a[0] == "#N" and a[1] == "canvas":
            if not stack and len(a) >= 7:
                c = Canvas("(root)", int(a[4]), int(a[5]), int(float(a[6])))
            elif len(a) >= 7:
                # subpatch: "#N canvas x y w h <name> <open>"
                parent_font = stack[-1].font if stack else 12
                c = Canvas(a[6], int(a[4]), int(a[5]), parent_font)
            else:
                c = Canvas("(unknown)", 0, 0, 12)
            stack.append(c)
            canvases.append(c)
            if root is None:
                root = c
            continue
        if not stack:
            continue
        cur = stack[-1]
        if a[0] == "#X" and a[1] == "restore":
            stack.pop()
            if stack:
                stack[-1].boxes.append(
                    pd_box("obj", int(float(a[2])), int(float(a[3])), a[4:],
                           stack[-1].font))
            continue
        if a[0] != "#X":
            continue
        if a[1] == "connect" and len(a) >= 6:
            cur.connects.append(tuple(int(v) for v in a[2:6]))
            continue
        if a[1] in ("obj", "msg", "text", "floatatom", "symbolatom", "listbox"):
            try:
                x, y = int(float(a[2])), int(float(a[3]))
            except ValueError:
                continue
            cur.boxes.append(pd_box(a[1], x, y, a[4:], cur.font))
    return canvases


def overlap(a, b):
    return not (a["x"] + a["w"] <= b["x"] or b["x"] + b["w"] <= a["x"]
                or a["y"] + a["h"] <= b["y"] or b["y"] + b["h"] <= a["y"])


def pd_check(path):
    issues = []
    for c in pd_parse(path):
        where = "" if c.name == "(root)" else "[%s] " % c.name
        fg = [b for b in c.boxes if b["kind"] not in PD_BACKGROUND]
        for i in range(len(fg)):
            for j in range(i + 1, len(fg)):
                if overlap(fg[i], fg[j]):
                    issues.append(
                        "%soverlap: [%s]%r at (%d,%d,%dx%d) vs [%s]%r at (%d,%d,%dx%d)"
                        % (where, fg[i]["kind"], fg[i]["text"][:32], fg[i]["x"],
                           fg[i]["y"], fg[i]["w"], fg[i]["h"], fg[j]["kind"],
                           fg[j]["text"][:32], fg[j]["x"], fg[j]["y"], fg[j]["w"],
                           fg[j]["h"]))
        for b in fg:
            if (b["x"] < 0 or b["y"] < 0 or b["x"] + b["w"] > c.w
                    or b["y"] + b["h"] > c.h):
                issues.append("%soutside canvas (%dx%d): [%s]%r at (%d,%d,%dx%d)"
                              % (where, c.w, c.h, b["kind"], b["text"][:32],
                                 b["x"], b["y"], b["w"], b["h"]))
        n = len(c.boxes)
        for (src, so, dst, di) in c.connects:
            if not (0 <= src < n and 0 <= dst < n):
                issues.append("%sconnect out of range: %d %d %d %d (%d boxes)"
                              % (where, src, so, dst, di, n))
    return issues


# --------------------------------------------------------------------------
# Max/MSP
# --------------------------------------------------------------------------

# Max renders a `comment` into its patching_rect and does NOT auto-size it, so
# text longer than the rect is silently clipped. Arial 12 (the help-patch
# default) averages ~6.2 px per character with a 21 px line box; we use a
# slightly conservative width so we flag only genuine clipping.
MAX_CHAR_W = 6.2
MAX_LINE_H = 18.0

# Boxes that are decoration and may legitimately sit under other boxes.
MAX_BACKGROUND = {"panel", "bpatcher"}
# Boxes with no visual extent worth checking.
MAX_SKIP = {"inlet", "outlet"}


def max_text_fits(text, w, linecount):
    if not text:
        return True
    per_line = max(1, int(w / MAX_CHAR_W))
    need = pd_wrap(text, per_line)
    return need <= max(1, linecount)


def max_check(path):
    issues = []
    try:
        doc = json.load(open(path, encoding="utf-8", errors="replace"))
    except Exception as e:                                    # noqa: BLE001
        return ["invalid JSON: %s" % e]
    p = doc.get("patcher")
    if not isinstance(p, dict):
        return ["no 'patcher' object"]

    boxes = {}
    rects = []
    for entry in p.get("boxes", []):
        b = entry.get("box", {})
        bid = b.get("id")
        if not bid:
            issues.append("box without id: %r" % (b.get("maxclass"),))
            continue
        if bid in boxes:
            issues.append("duplicate box id %s" % bid)
        boxes[bid] = b
        r = b.get("patching_rect")
        cls = b.get("maxclass", "")
        if not r or len(r) != 4:
            if "patcher" not in b:
                issues.append("box %s (%s) has no patching_rect" % (bid, cls))
            continue
        x, y, w, h = (float(v) for v in r)
        if w <= 0 or h <= 0:
            issues.append("box %s (%s) has an empty rect %r" % (bid, cls, r))
        if cls == "comment":
            lc = int(b.get("linecount", 1) or 1)
            if not max_text_fits(b.get("text", ""), w, lc):
                issues.append("clipped comment %s: %r does not fit %.0fx%.0f "
                              "(linecount=%d)" % (bid, b.get("text", "")[:48], w, h, lc))
            if lc > 1 and h < lc * MAX_LINE_H - 2:
                issues.append("comment %s: linecount=%d but height is only %.0f"
                              % (bid, lc, h))
        if cls not in MAX_BACKGROUND and cls not in MAX_SKIP:
            rects.append((bid, cls, x, y, w, h))

    for i in range(len(rects)):
        for j in range(i + 1, len(rects)):
            a, bb = rects[i], rects[j]
            ra = dict(x=a[2], y=a[3], w=a[4], h=a[5])
            rb = dict(x=bb[2], y=bb[3], w=bb[4], h=bb[5])
            if overlap(ra, rb):
                issues.append("overlap: %s(%s) at (%.0f,%.0f,%.0fx%.0f) vs "
                              "%s(%s) at (%.0f,%.0f,%.0fx%.0f)"
                              % (a[0], a[1], a[2], a[3], a[4], a[5],
                                 bb[0], bb[1], bb[2], bb[3], bb[4], bb[5]))

    for entry in p.get("lines", []):
        pl = entry.get("patchline", {})
        for end in ("source", "destination"):
            ref = pl.get(end)
            if not (isinstance(ref, list) and len(ref) == 2):
                issues.append("patchline with a malformed %s: %r" % (end, ref))
                continue
            bid, port = ref[0], int(ref[1])
            b = boxes.get(bid)
            if b is None:
                issues.append("patchline %s references unknown box %s" % (end, bid))
                continue
            key = "numoutlets" if end == "source" else "numinlets"
            count = int(b.get(key, 0) or 0)
            if port < 0 or port >= count:
                issues.append("patchline %s %s port %d out of range (%s=%d, %s)"
                              % (end, bid, port, key, count, b.get("maxclass")))
    return issues


# --------------------------------------------------------------------------

def run(kind, paths, checker, verbose):
    bad = 0
    total_issues = 0
    report = {}
    for path in sorted(paths):
        issues = checker(path)
        report[os.path.basename(path)] = issues
        if issues:
            bad += 1
            total_issues += len(issues)
            print("%-52s %d issue(s)" % (os.path.basename(path), len(issues)))
            if verbose:
                for msg in issues[:12]:
                    print("    " + msg)
                if len(issues) > 12:
                    print("    ... and %d more" % (len(issues) - 12))
    print("[%s] %d/%d patches with problems, %d issues total"
          % (kind, bad, len(paths), total_issues))
    return bad, report


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pd", metavar="DIR", help="directory holding *-help.pd")
    ap.add_argument("--max", metavar="DIR", help="directory holding *.maxhelp")
    ap.add_argument("--json", metavar="FILE", help="write a machine-readable report")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if not args.pd and not args.max:
        ap.error("nothing to check: pass --pd and/or --max")

    bad = 0
    report = {}
    if args.pd:
        files = glob.glob(os.path.join(args.pd, "*-help.pd")) or \
                glob.glob(os.path.join(args.pd, "*.pd"))
        n, report["pd"] = run("pd", files, pd_check, args.verbose)
        bad += n
    if args.max:
        files = glob.glob(os.path.join(args.max, "*.maxhelp"))
        n, report["max"] = run("max", files, max_check, args.verbose)
        bad += n

    if args.json:
        with open(args.json, "w", encoding="utf-8") as fp:
            json.dump(report, fp, indent=1)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
