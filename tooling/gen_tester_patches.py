#!/usr/bin/env python3
"""Generate per-backend tester patches from Avendish dump JSON.

Every Avendish object emits ``dump/<c_name>.json`` describing its metadata and
ports (see AVND_TEST_HARNESS_PLAN.md). That JSON is the single contract this tool
consumes: for each object it places a driver on every input and a sink on every
output, so the object can be exercised by hand on a given backend.

Backends: max (.maxpat), pd (.pd), touchdesigner (.py), score (.js),
godot (.gd), python (.py), wasm (index.html + notes).
Usage:
    py gen_tester_patches.py --backend max --in <dump_dir_or_json> --out <out_dir>
"""

import argparse
import json
import os
import re
import sys

# When set, patches self-quit the host after loading (headless smoke test).
AUTOQUIT = False

# ---------------------------------------------------------------------------
# Dump-JSON helpers
# ---------------------------------------------------------------------------

def load_dumps(path):
    """Return a list of (name, dump_dict) from a file or a directory of *.json."""
    dumps = []
    if os.path.isdir(path):
        for fn in sorted(os.listdir(path)):
            if fn.endswith(".json"):
                with open(os.path.join(path, fn), encoding="utf-8") as f:
                    dumps.append((fn[:-5], json.load(f)))
    else:
        with open(path, encoding="utf-8") as f:
            dumps.append((os.path.splitext(os.path.basename(path))[0], json.load(f)))
    return dumps


def port_kind(p):
    """Coarse classification of a port from its dump entry."""
    t = p.get("type", "")
    if t == "texture":
        return "texture"
    if t in ("audio", "midi", "message", "callback"):
        return t
    if t == "parameter":
        w = p.get("parameter", {}).get("widget", "")
        vt = p.get("parameter", {}).get("value_type", "")
        if w in ("xy", "xyz", "xyzw", "rgb", "rgba") or w.startswith(("xy", "xyz", "xyzw")):
            return "vec"
        if vt == "enum" or w == "choices":
            return "enum"
        if vt in ("int", "float", "bool", "string"):
            return "number"
        return "param"
    return t or "unknown"


def vec_dim(p):
    w = p.get("parameter", {}).get("widget", "")
    if w.startswith("xyzw") or w.startswith("rgba"):
        return 4
    if w.startswith("xyz") or w.startswith("rgb"):
        return 3
    return 2


def is_control(p):
    pa = p.get("parameter", {})
    return bool(pa.get("control") or pa.get("value_port")) and port_kind(p) in (
        "number", "vec", "enum", "param")


def init_value(p):
    pa = p.get("parameter", {})
    rng = pa.get("range", {})
    v = rng["init"] if "init" in rng else pa.get("default", 0)
    if v is None:
        v = 0
    if isinstance(v, bool):  # Max/Pd want 0/1, not Python True/False
        return int(v)
    # Dump JSON stores floats at full double precision; round for legible patches.
    if isinstance(v, float):
        r = round(v, 6)
        return int(r) if r == int(r) and pa.get("value_type") == "int" else r
    return v


def sanitize_td_name(name):
    """Match avendish helpers.hpp sanitize_td_name: first char upper letter,
    rest lowercase letters/digits, others stripped; 'Param' if empty."""
    out, first = [], True
    for c in name:
        if first:
            if c.isascii() and c.isalpha():
                out.append(c.upper())
                first = False
        elif c.isascii() and (c.islower() or c.isdigit()):
            out.append(c)
        elif c.isascii() and c.isupper():
            out.append(c.lower())
    return "".join(out) or "Param"


# ---------------------------------------------------------------------------
# Max / MSP  (.maxpat is JSON)
# ---------------------------------------------------------------------------

def _max_box(bid, maxclass, text, x, y, w=120, h=22, n_in=1, n_out=1, outtype=None):
    box = {
        "id": bid,
        "maxclass": maxclass,
        "numinlets": n_in,
        "numoutlets": n_out,
        "patching_rect": [x, y, w, h],
    }
    if text is not None:
        box["text"] = text
    if outtype is not None:
        box["outlettype"] = outtype
    elif n_out > 0:
        box["outlettype"] = [""] * n_out
    return {"box": box}


def _max_line(src_id, src_out, dst_id, dst_in):
    return {"patchline": {"source": [src_id, src_out], "destination": [dst_id, dst_in]}}


def gen_max(name, dump):
    c_name = dump.get("metadatas", {}).get("c_name", name)
    inputs = dump.get("inputs", [])
    outputs = dump.get("outputs", [])

    boxes, lines = [], []
    n = 0
    def nid():
        nonlocal n
        n += 1
        return f"obj-{n}"

    obj_id = nid()
    obj_y = 320
    boxes.append(_max_box(obj_id, "newobj", c_name, 40, obj_y, 220, 22,
                          n_in=max(1, len(inputs)), n_out=max(1, len(outputs))))

    # loadbang self-drives the patch: loading it instantiates the object and sends
    # one set of inputs, so a headless load is a smoke test (instantiation + cook).
    lb = nid()
    boxes.append(_max_box(lb, "newobj", "loadbang", 40, 20, 70, 22, outtype=["bang"]))
    mark = nid()
    boxes.append(_max_box(mark, "newobj", "print avnd_test", 140, 20, 120, 22, n_out=0, outtype=[]))
    lines.append(_max_line(lb, 0, mark, 0))

    x = 40
    for p in inputs:
        k = port_kind(p)
        pname = p.get("name", "in")
        if k == "texture":
            src = nid()
            boxes.append(_max_box(src, "newobj", "jit.noise 4 char 64 64", x, 120, 150, 22,
                                  n_in=1, n_out=1, outtype=["jit_matrix"]))
            mtr = nid()
            boxes.append(_max_box(mtr, "newobj", "metro 33", x, 80, 80, 22, outtype=["bang"]))
            start = nid()
            boxes.append(_max_box(start, "message", "1", x, 50, 30, 22))
            lines.append(_max_line(lb, 0, start, 0))
            lines.append(_max_line(start, 0, mtr, 0))
            lines.append(_max_line(mtr, 0, src, 0))
            lines.append(_max_line(src, 0, obj_id, 0))
        elif k in ("number", "vec", "enum", "param"):
            v = init_value(p)
            src = nid()
            boxes.append(_max_box(src, "message", f"{pname} {v}", x, 200, 140, 22))
            lines.append(_max_line(src, 0, obj_id, 0))
            lines.append(_max_line(lb, 0, src, 0))  # fire on load
            x += 150

    sx = 40
    for i, p in enumerate(outputs):
        k = port_kind(p)
        if k == "texture":
            snk = nid()
            boxes.append(_max_box(snk, "newobj", "jit.pwindow", sx, obj_y + 80, 160, 120,
                                  n_in=1, n_out=1))
            lines.append(_max_line(obj_id, 0, snk, 0))
            sx += 180
        else:
            snk = nid()
            boxes.append(_max_box(snk, "newobj", f"print {p.get('name','out')}", sx,
                                  obj_y + 80, 140, 22, n_out=0, outtype=[]))
            lines.append(_max_line(obj_id, min(i, max(0, len(outputs) - 1)), snk, 0))
            sx += 150

    if AUTOQUIT:  # loadbang -> del 1500 -> [; max quit] for a headless smoke test
        dl = nid()
        boxes.append(_max_box(dl, "newobj", "del 1500", 480, 60, 70, 22, outtype=["bang"]))
        q = nid()
        boxes.append(_max_box(q, "message", "; max quit", 480, 100, 90, 22))
        lines.append(_max_line(lb, 0, dl, 0))
        lines.append(_max_line(dl, 0, q, 0))

    patch = {
        "patcher": {
            "fileversion": 1,
            "appversion": {"major": 8, "minor": 5, "revision": 0, "architecture": "x64",
                            "modernui": 1},
            "classnamespace": "box",
            "rect": [80.0, 80.0, 900.0, 600.0],
            "boxes": boxes,
            "lines": lines,
        }
    }
    return json.dumps(patch, indent=2), ".maxpat"


# ---------------------------------------------------------------------------
# Pure Data  (.pd netlist)
# ---------------------------------------------------------------------------

def gen_pd(name, dump):
    c_name = dump.get("metadatas", {}).get("c_name", name)
    inputs = dump.get("inputs", [])
    outputs = dump.get("outputs", [])

    obj_y = 320
    lines = ["#N canvas 80 80 900 600 12;",
             f"#X obj 40 {obj_y} {c_name};",  # idx 0
             "#X obj 40 20 loadbang;"]        # idx 1, self-drives on load
    obj_idx, lb_idx, idx = 0, 1, 2
    conns = []
    x = 40
    for p in inputs:
        if port_kind(p) in ("number", "vec", "enum", "param"):
            lines.append(f"#X msg {x} 200 {p.get('name','in')} {init_value(p)};")
            conns.append((idx, 0, obj_idx, 0))   # msg -> object
            conns.append((lb_idx, 0, idx, 0))    # loadbang -> msg
            idx += 1
            x += 120

    sx = 40
    for p in outputs:
        if port_kind(p) == "texture":
            continue
        lines.append(f"#X obj {sx} {obj_y + 80} print {p.get('name','out')};")
        conns.append((obj_idx, 0, idx, 0))
        idx += 1
        sx += 130

    if AUTOQUIT:  # loadbang -> del 500 -> [; pd quit] so a headless run exits 0
        q, dl = idx, idx + 1
        lines.append("#X msg 700 540 \\; pd quit;")
        lines.append("#X obj 700 500 delay 500;")
        conns.append((lb_idx, 0, dl, 0))
        conns.append((dl, 0, q, 0))

    for (s, so, d, di) in conns:
        lines.append(f"#X connect {s} {so} {d} {di};")
    return "\n".join(lines) + "\n", ".pd"


# ---------------------------------------------------------------------------
# TouchDesigner  (.py to run inside TD)
# ---------------------------------------------------------------------------

def gen_td(name, dump):
    md = dump.get("metadatas", {})
    c_name = md.get("c_name", name)
    inputs = dump.get("inputs", [])
    outputs = dump.get("outputs", [])
    is_top = any(port_kind(p) == "texture" for p in inputs + outputs)
    fam = "TOP" if is_top else "CHOP"
    optype = sanitize_td_name(md.get("name", c_name))
    obj = "tester_" + c_name

    L = [
        "# TouchDesigner tester. Paste into a DAT, then right-click > Run Script.",
        f"# Needs the avendish Custom OP plugin registering opType '{optype}'.",
        f"# Dispatches to a {fam} Custom OP (TOP for texture, CHOP otherwise).",
        "p = parent()",
        f"n = p.create('{optype}', '{obj}')  # 2nd arg = registered opType",
    ]
    x = 0
    for p_ in inputs:
        k = port_kind(p_)
        nm, drv = sanitize_td_name(p_.get("name", "in")), f"drv_{x}"
        if k == "texture":
            L.append(f"src = p.create(noiseTOP, '{drv}'); src.nodeY = {x * -150}")
            L.append("n.inputConnectors[0].connect(src)")
        elif is_control(p_):
            v = init_value(p_)
            if not is_top:
                L.append(f"c = p.create(constantCHOP, '{drv}'); c.par.value0 = {td_lit(v)}")
            L.append(f"n.par.{nm} = {td_lit(v)}  # input '{p_.get('name')}'")
        x += 1
    for p_ in outputs:
        if port_kind(p_) == "texture":
            L.append(f"out = p.create(nullTOP, 'out_{sanitize_td_name(p_.get('name','out'))}'); out.inputConnectors[0].connect(n)")
        else:
            L.append(f"# output '{p_.get('name','out')}' read via n.par or CHOP channel")
    return "\n".join(L) + "\n", ".py"


def td_lit(v):
    if isinstance(v, str):
        return repr(v)
    if isinstance(v, list):
        return repr(v[0]) if v else "0"
    return v


# ---------------------------------------------------------------------------
# ossia score  (.js for the EditJsContext scripting API; global is `Score`)
# ---------------------------------------------------------------------------

def _score_setval(var, p):
    """JS Score.setValue() call(s) for a control port, picking the overload."""
    k, nm = port_kind(p), p.get("name", "")
    port = f'Score.port({var}, {json.dumps(nm)})'
    if k == "vec":
        v = init_value(p)
        d = vec_dim(p)
        comp = ", ".join([str(v if not isinstance(v, list) else (v[0] if v else 0))] * d)
        return f'Score.setValue({port}, Qt.vector{d}d({comp}));'
    if k == "enum":
        ch = p.get("parameter", {}).get("choices", [])
        val = json.dumps(ch[0]) if ch else "0"
        return f'Score.setValue({port}, {val});'
    v = init_value(p)
    if isinstance(v, str):
        return f'Score.setValue({port}, {json.dumps(v)});'
    if isinstance(v, bool):
        return f'Score.setValue({port}, {"true" if v else "false"});'
    return f'Score.setValue({port}, {v});'


def gen_score(name, dump):
    md = dump.get("metadatas", {})
    pname = md.get("name", md.get("c_name", name))
    inputs = dump.get("inputs", [])
    L = [
        "// ossia score tester (run via the JS console / EditContext).",
        f"// Process name {json.dumps(pname)} must appear in Score.availableProcesses().",
        "var root = Score.rootInterval();",
        f"var obj = Score.createProcess(root, {json.dumps(pname)}, \"\");",
        "Score.createBox(obj, 0, 1000, 0);",
    ]
    for p in inputs:
        if is_control(p):
            L.append(_score_setval("obj", p))
    L.append("Score.play();")
    L.append("// Score.serializeAsJson() returns the resulting document.")
    return "\n".join(L) + "\n", ".js"


# ---------------------------------------------------------------------------
# Godot  (.gd GDScript; node class is avnd_<c_name>, props named by port)
# ---------------------------------------------------------------------------

def _gd_lit(p):
    k, v = port_kind(p), init_value(p)
    if k == "vec":
        d = vec_dim(p)
        return f"Vector{d}(" + ", ".join([str(v if not isinstance(v, list) else (v[0] if v else 0))] * d) + ")"
    if k == "enum":
        return "0"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, str):
        return json.dumps(v)
    if isinstance(v, list):
        return str(v[0]) if v else "0"
    return str(v)


def gen_godot(name, dump):
    c_name = dump.get("metadatas", {}).get("c_name", name)
    cls = f"avnd_{c_name}"  # node.hpp: avnd_<C_NAME> (suffix _geo/_fx for geometry/audio)
    inputs = dump.get("inputs", [])
    L = [
        "extends Node",
        "# Godot tester. Requires the avendish GDExtension for this object.",
        f"# Class derives from C_NAME: '{cls}' (+_geo/_fx suffix for geometry/audio).",
        "func _ready():",
        f"\tvar n = ClassDB.instantiate(\"{cls}\")",
        "\tadd_child(n)",
    ]
    for p in inputs:
        if is_control(p):
            prop = p.get("name", "in")
            L.append(f'\tn.set({json.dumps(prop)}, {_gd_lit(p)})')
    if not any(is_control(p) for p in inputs):
        L.append("\tpass")
    return "\n".join(L) + "\n", ".gd"


# ---------------------------------------------------------------------------
# Python  (CPython module py<c_name>, class <c_name>, props per port)
# ---------------------------------------------------------------------------

def _py_lit(p):
    k, v = port_kind(p), init_value(p)
    if k == "vec":
        d = vec_dim(p)
        base = v[0] if isinstance(v, list) and v else (0 if isinstance(v, list) else v)
        return repr([base] * d)
    if k == "enum":
        return "0"
    return repr(v)


def gen_python(name, dump):
    c_name = dump.get("metadatas", {}).get("c_name", name)
    inputs = dump.get("inputs", [])
    outputs = dump.get("outputs", [])
    mod, cls = f"py{c_name}", c_name  # prototype.cpp.in / processor.hpp
    L = [
        "#!/usr/bin/env python3",
        f"import {mod}",
        f"o = {mod}.{cls}()",
    ]
    for p in inputs:
        if is_control(p):
            L.append(f"o.{_py_attr(p)} = {_py_lit(p)}")
    L.append("o.process()")
    for p in outputs:
        if port_kind(p) != "texture":
            L.append(f"print({json.dumps(p.get('name',''))}, o.{_py_attr(p)})")
    return "\n".join(L) + "\n", ".py"


def _py_attr(p):
    """Property name: the port name as a Python identifier."""
    nm = p.get("name", "")
    return re.sub(r"\W", "_", nm) or "x"


# ---------------------------------------------------------------------------
# WASM  (per-object page is <c_name>.html; we emit a gallery index.html)
# ---------------------------------------------------------------------------

def gen_wasm_index(dumps):
    rows = []
    for name, dump in dumps:
        md = dump.get("metadatas", {})
        c_name = md.get("c_name", name)
        disp = md.get("name", c_name)
        rows.append(f'    <li><a href="./{c_name}.html">{disp}</a> '
                    f'<code>{c_name}</code></li>')
    body = "\n".join(rows)
    return (
        "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"/>"
        "<title>Avendish WASM testers</title></head><body>\n"
        "<h1>Avendish WASM testers</h1>\n"
        "<!-- Each object builds to <c_name>.html (avendish.wasm.cmake). -->\n"
        f"<ul>\n{body}\n</ul>\n</body></html>\n"
    )


# ---------------------------------------------------------------------------
# Headless test runners (one script over all dumps): create/cook/import each
# object and write a JSON pass/fail report. These automate the instantiation +
# cook smoke test that catches crashes/load errors per backend.
# ---------------------------------------------------------------------------

def gen_td_runner(dumps):
    specs = []
    for name, dump in dumps:
        md = dump.get("metadatas", {})
        ins = dump.get("inputs", [])
        is_top = any(port_kind(p) == "texture" for p in ins + dump.get("outputs", []))
        pars = {sanitize_td_name(p["name"]): init_value(p)
                for p in ins if is_control(p) and "name" in p}
        # The registered opType is sanitize_td_name(C_NAME) -- the TD binding's
        # configure_opInfo() is called with @AVND_C_NAME@, not the display name.
        # 'file' is the golden JSON basename (the dump/target name).
        specs.append({"name": md.get("c_name", name),
                      "type": sanitize_td_name(md.get("c_name", name)),
                      "family": "TOP" if is_top else "CHOP",
                      "file": name, "pars": pars})
    return (
        "# Run inside TouchDesigner (an Execute DAT). For each avendish Custom OP:\n"
        "# resolve its class (opType+family), apply the golden input controls (from\n"
        "# AVND_TD_GOLDEN_DIR/<file>.json if set), cook, and CAPTURE the output CHOP\n"
        "# channels into the report. The harness diffs those against the golden.\n"
        "# Everything is inlined (no def/comprehension): this is exec()'d in an\n"
        "# Execute DAT callback, where nested scopes can't see these locals.\n"
        "import json, os\n"
        f"SPECS = {json.dumps(specs, indent=2)}\n"
        "FAMILIES = ['CHOP', 'TOP', 'SOP', 'POP', 'DAT', 'MAT']\n"
        "GDIR = os.environ.get('AVND_TD_GOLDEN_DIR', '')\n"
        "container = op('/').create(baseCOMP, 'avnd_sweep')\n"
        "report = []\n"
        "RP = project.folder + '/td_test_report.json'\n"
        "CUR = project.folder + '/td_current.txt'\n"
        "try:\n"
        "    report = json.load(open(RP))\n"
        "except Exception:\n"
        "    report = []\n"
        "done = set()\n"
        "for r in report: done.add(r['name'])\n"
        "for s in SPECS:\n"
        "    if s['name'] in done: continue  # resume after a crash\n"
        "    open(CUR, 'w').write(s['name'])  # crash breadcrumb\n"
        "    cls = None\n"
        "    fams = []\n"
        "    if s.get('family'): fams.append(s['family'])\n"
        "    for f in FAMILIES:\n"
        "        if f != s.get('family'): fams.append(f)\n"
        "    for fam in fams:\n"
        "        try:\n"
        "            cls = eval(s['type'] + fam); break\n"
        "        except Exception:\n"
        "            cls = None\n"
        "    if cls is None:\n"
        "        report.append({'name': s['name'], 'ok': False,\n"
        "                       'exception': 'class not found for ' + s['type']})\n"
        "        open(RP, 'w').write(json.dumps(report, indent=2))\n"
        "        continue\n"
        "    golden = None\n"
        "    if GDIR:\n"
        "        try:\n"
        "            golden = json.load(open(os.path.join(GDIR, s['file'] + '.json')))\n"
        "        except Exception:\n"
        "            golden = None\n"
        "    entry = {'name': s['name'], 'ok': False}\n"
        "    try:\n"
        "        n = container.create(cls, 'test_' + s['name'])\n"
        "        # Apply golden input controls as params; ones that aren't params\n"
        "        # (value-input ports become input CHANNELS in TD) are collected to\n"
        "        # feed via a source CHOP below.\n"
        "        in_names = []\n"
        "        in_vals = []\n"
        "        if golden:\n"
        "            for c in golden.get('inputs', {}).get('controls', []):\n"
        "                as_par = False\n"
        "                try:\n"
        "                    setattr(n.par, c['name'], c['value']); as_par = True\n"
        "                except Exception:\n"
        "                    try:\n"
        "                        setattr(n.par, c['name'].capitalize(), c['value'])\n"
        "                        as_par = True\n"
        "                    except Exception:\n"
        "                        as_par = False\n"
        "                if not as_par:\n"
        "                    in_names.append(c['name'])\n"
        "                    in_vals.append(c['value'])\n"
        "        else:\n"
        "            for k, v in s['pars'].items():\n"
        "                try: setattr(n.par, k, v)\n"
        "                except Exception: pass\n"
        "        gin = None\n"
        "        if golden: gin = golden.get('inputs', {}).get('audio')\n"
        "        entry['has_audio_in'] = bool(gin)\n"
        "        # Feed a source CHOP so value-IO objects can cook. Value inputs\n"
        "        # (ports that aren't params) become input channels -> Constant CHOP.\n"
        "        # Audio (time-varying) inputs need a Script CHOP -- via samples in\n"
        "        # a Table DAT the script reads.\n"
        "        src = None\n"
        "        try:\n"
        "            if gin:\n"
        "                tab = container.create(tableDAT, 'srcd_' + s['name'])\n"
        "                tab.clear()\n"
        "                for chn in gin:\n"
        "                    row = []\n"
        "                    for x in chn: row.append(repr(float(x)))\n"
        "                    tab.appendRow(row)\n"
        "                src = container.create(scriptCHOP, 'src_' + s['name'])\n"
        "                cb = container.create(textDAT, 'srccb_' + s['name'])\n"
        "                cb.text = ('def onCook(scriptOp):\\n'\n"
        "                           '    scriptOp.clear()\\n'\n"
        "                           '    t = op(scriptOp.fetch(\"tab\",\"\"))\\n'\n"
        "                           '    if not t: return\\n'\n"
        "                           '    scriptOp.numSamples = t.numCols\\n'\n"
        "                           '    for r in range(t.numRows):\\n'\n"
        "                           '        c = scriptOp.appendChan(\"c\"+str(r))\\n'\n"
        "                           '        for j in range(t.numCols):\\n'\n"
        "                           '            c[j] = float(t[r,j].val)\\n')\n"
        "                src.store('tab', tab.path)\n"
        "                src.par.callbacks = cb\n"
        "            elif in_vals:\n"
        "                src = container.create(constantCHOP, 'src_' + s['name'])\n"
        "                for j in range(len(in_vals)):\n"
        "                    try:\n"
        "                        setattr(src.par, 'value' + str(j), float(in_vals[j]))\n"
        "                        setattr(src.par, 'name' + str(j), str(in_names[j]))\n"
        "                    except Exception: pass\n"
        "            if src is not None:\n"
        "                src.cook(force=True)\n"
        "                if len(n.inputConnectors) > 0:\n"
        "                    n.inputConnectors[0].connect(src)\n"
        "        except Exception as e:\n"
        "            entry['input_err'] = str(e)\n"
        "        n.cook(force=True)\n"
        "        entry['ok'] = not n.errors()\n"
        "        entry['errors'] = n.errors()\n"
        "        entry['warnings'] = n.warnings()\n"
        "        # Capture output channels WITH names: control outputs surface as\n"
        "        # named channels (e.g. 'Out'), audio outputs as ordered channels.\n"
        "        td_out = []\n"
        "        try:\n"
        "            ns = n.numSamples\n"
        "            for ch in n.chans():\n"
        "                samples = []\n"
        "                for i in range(ns): samples.append(float(ch[i]))\n"
        "                td_out.append({'name': ch.name, 'samples': samples})\n"
        "            entry['td_out'] = td_out\n"
        "        except Exception:\n"
        "            entry['td_out'] = None\n"
        "        n.destroy()\n"
        "    except Exception as e:\n"
        "        entry['exception'] = str(e)\n"
        "    report.append(entry)\n"
        "    open(RP, 'w').write(json.dumps(report, indent=2))\n"
        "open(CUR, 'w').write('DONE')\n"
        "print('avnd td report:', sum(1 for r in report if r['ok']), '/', len(report), 'ok')\n"
    ), ".py"


def gen_python_runner(dumps):
    specs = []
    for name, dump in dumps:
        md = dump.get("metadatas", {})
        c = md.get("c_name", name)
        pars = {_py_attr(p): init_value(p) for p in dump.get("inputs", [])
                if is_control(p)}
        specs.append({"name": c, "mod": "py" + c, "cls": c, "pars": pars})
    return (
        "#!/usr/bin/env python3\n"
        "# Imports each avendish python module, builds, sets inputs, calls; reports.\n"
        "import importlib, json, sys, traceback\n"
        f"SPECS = {json.dumps(specs, indent=2)}\n"
        "report = []\n"
        "for s in SPECS:\n"
        "    try:\n"
        "        m = importlib.import_module(s['mod'])\n"
        "        o = getattr(m, s['cls'])()\n"
        "        for k, v in s['pars'].items():\n"
        "            try: setattr(o, k, v)\n"
        "            except Exception: pass\n"
        "        if hasattr(o, 'process'): o.process()\n"
        "        report.append({'name': s['name'], 'ok': True})\n"
        "    except Exception as e:\n"
        "        report.append({'name': s['name'], 'ok': False, 'error': repr(e)})\n"
        "ok = sum(1 for r in report if r['ok'])\n"
        "print(json.dumps(report, indent=2))\n"
        "print(f'{ok}/{len(report)} ok')\n"
        "sys.exit(0 if ok == len(report) else 1)\n"
    ), ".py"


GENERATORS = {
    "max": gen_max, "pd": gen_pd, "touchdesigner": gen_td,
    "score": gen_score, "godot": gen_godot, "python": gen_python,
}
# Aggregate backends produce a single file over all dumps.
AGGREGATE = {"wasm", "td-runner", "python-runner"}
AGG_GEN = {
    "wasm": (gen_wasm_index, "index.html"),
    "td-runner": (gen_td_runner, "td_run_all.py"),
    "python-runner": (gen_python_runner, "py_run_all.py"),
}


def _emit_aggregate(backend, dumps, out):
    gen, fname = AGG_GEN[backend]
    content = gen(dumps) if backend == "wasm" else gen(dumps)[0]
    outp = os.path.join(out, fname)
    with open(outp, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    print(f"  + {outp}")


def _emit_per_object(backend, dumps, out):
    gen = GENERATORS[backend]
    count = 0
    for name, dump in dumps:
        try:
            content, ext = gen(name, dump)
        except Exception as e:
            print(f"  ! {name}: {e}", file=sys.stderr)
            continue
        outp = os.path.join(out, name + ext)
        with open(outp, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)
        count += 1
    print(f"  {count} {backend} patch(es) -> {out}")


# Backend -> subdir for the `all` layout.
ALL_BACKENDS = ["max", "pd", "touchdesigner", "score", "godot", "python",
                "wasm", "td-runner", "python-runner"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", required=True,
                    choices=sorted(set(GENERATORS) | AGGREGATE | {"all"}))
    ap.add_argument("--in", dest="inp", required=True, help="dump JSON file or directory")
    ap.add_argument("--out", dest="out", required=True, help="output directory")
    ap.add_argument("--autoquit", action="store_true",
                    help="patches self-quit the host after loading (headless)")
    args = ap.parse_args()

    global AUTOQUIT
    AUTOQUIT = args.autoquit
    dumps = load_dumps(args.inp)
    os.makedirs(args.out, exist_ok=True)

    if args.backend == "all":
        for b in ALL_BACKENDS:
            sub = os.path.join(args.out, b)
            os.makedirs(sub, exist_ok=True)
            if b in AGGREGATE:
                _emit_aggregate(b, dumps, sub)
            else:
                _emit_per_object(b, dumps, sub)
        print(f"all backends generated for {len(dumps)} objects under {args.out}")
        return

    if args.backend in AGGREGATE:
        _emit_aggregate(args.backend, dumps, args.out)
        print(f"1 {args.backend} file ({len(dumps)} objects) -> {args.out}")
        return

    _emit_per_object(args.backend, dumps, args.out)


if __name__ == "__main__":
    main()
