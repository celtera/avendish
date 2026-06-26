"""Reusable helpers for unit-testing avendish objects through the Python backend.

Each avendish object compiled with the `python` backend imports as a module
`py<c_name>` exposing a class `<c_name>`. Ports surface as properties; the
object is run with `process()` (value/data objects) or `process_audio()`
(audio objects).

Port -> Python mapping
----------------------
- control / value ports : plain attributes (float/int/bool/str/enum/aggregate)
- tensor ports          : numpy arrays (zero-copy view / resize+copy)
- texture ports         : (H, W, C) numpy (uint8 or float32 by format)
- buffer ports          : 1-D numpy (uint8 bytes for raw/gpu, element dtype for typed)
- midi ports            : list of (bytes, timestamp)
- dynamic ports         : list of sub-port values (assigning resizes)
- file ports            : Python bytes
- audio                 : obj.process_audio(ndarray[channels, frames]) -> ndarray

Usage
-----
    import py_test_harness as h
    m = h.load("avnd_test_audio_mono", module_dir="path/to/built/modules")
    o = m()
    o.Gain = 2.0
    out = o.process_audio(np.ones((1, 64), dtype=np.float32))
"""

import importlib
import os
import sys


def load(c_name, module_dir=None):
    """Import the module for an object's c_name and return its class."""
    if module_dir and module_dir not in sys.path:
        sys.path.insert(0, module_dir)
    mod = importlib.import_module("py" + c_name)
    return getattr(mod, c_name)


def ports(obj):
    """Names of the object's exposed ports/methods (excludes dunder/internal)."""
    return [a for a in dir(obj)
            if not a.startswith("__") and not a.startswith("_pybind11")]


def is_audio(obj):
    return "process_audio" in dir(obj)


def run(obj):
    """Run one tick: process_audio is audio-only, so use process() here."""
    if hasattr(obj, "process"):
        obj.process()


def discover(module_dir):
    """Yield (c_name, class) for every py<c_name> module in a directory."""
    if module_dir not in sys.path:
        sys.path.insert(0, module_dir)
    for fn in sorted(os.listdir(module_dir)):
        if fn.startswith("py") and fn.split(".")[0][2:]:
            base = fn.split(".")[0]
            if not (fn.endswith(".pyd") or fn.endswith(".so")):
                continue
            c_name = base[2:]
            try:
                mod = importlib.import_module(base)
                cls = getattr(mod, c_name, None)
                if cls is not None:
                    yield c_name, cls
            except Exception as e:  # pragma: no cover - diagnostic
                print(f"  [skip] {base}: {e}", file=sys.stderr)
