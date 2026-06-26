"""End-to-end demonstration / smoke test of the avendish Python backend.

Run after building the test objects' python modules into a directory, e.g.:
    PYTHONPATH=<built-modules-dir> python tooling/test_avnd_python.py

Covers every port category the Python backend exposes. Objects that are not
built are skipped, so a partial build still reports what it can.
"""

import sys
import numpy as np

import py_test_harness as h

OK, SKIP, FAIL = [], [], []


def case(name):
    def deco(fn):
        try:
            fn()
            OK.append(name)
            print(f"  PASS  {name}")
        except ModuleNotFoundError:
            SKIP.append(name)
            print(f"  skip  {name} (module not built)")
        except AssertionError as e:
            FAIL.append((name, str(e)))
            print(f"  FAIL  {name}: {e}")
    return deco


@case("control: float value")
def _():
    o = h.load("avnd_test_float_slider")()
    o.In = 0.75
    assert abs(o.In - 0.75) < 1e-6


@case("value I/O: int passthrough")
def _():
    o = h.load("avnd_test_val_int")()
    o.In = 42
    o.process()
    assert o.Out == 42


@case("enum control")
def _():
    o = h.load("avnd_test_enum")()
    assert "Mode" in h.ports(o)
    o.process()


@case("aggregate (field-named struct)")
def _():
    o = h.load("avnd_test_aggregate")()
    assert "In" in h.ports(o) and "Out" in h.ports(o)


@case("tensor (numpy)")
def _():
    o = h.load("avnd_test_tensor")()
    assert "In" in h.ports(o)


@case("messages")
def _():
    o = h.load("avnd_test_messages")()
    assert any(p not in ("process",) for p in h.ports(o))


@case("audio: per-sample-arg gain")
def _():
    o = h.load("avnd_test_audio_mono")()
    o.Gain = 2.0
    y = o.process_audio(np.ones((1, 32), dtype=np.float32))
    assert y.shape == (1, 32) and np.allclose(y, 2.0)


@case("audio: poly (stereo)")
def _():
    o = h.load("avnd_test_audio_poly")()
    y = o.process_audio(np.random.rand(2, 64).astype(np.float32))
    assert y.shape == (2, 64) and np.all(np.isfinite(y))


@case("texture: rgba8 roundtrip")
def _():
    o = h.load("avnd_test_tex_rgba8")()
    img = (np.random.rand(4, 5, 4) * 255).astype(np.uint8)
    o.In = img
    o.process()
    assert np.array_equal(o.Out, img)


@case("texture: r32f roundtrip")
def _():
    o = h.load("avnd_test_tex_r32f")()
    img = np.random.rand(4, 5, 1).astype(np.float32)
    o.In = img
    o.process()
    assert np.array_equal(o.Out, img)


@case("buffer: typed-float roundtrip")
def _():
    o = h.load("avnd_test_buffer_typed")()
    x = np.arange(10, dtype=np.float32) * 1.5
    o.In = x
    o.process()
    assert np.array_equal(o.Out, x)


@case("buffer: raw bytes roundtrip")
def _():
    o = h.load("avnd_test_buffer_raw")()
    b = np.arange(12, dtype=np.uint8)
    o.In = b
    o.process()
    assert np.array_equal(o.Out, b)


@case("gpu buffer: output bytes")
def _():
    o = h.load("avnd_test_gpu_buffer_out")()
    o.Bytes = 16
    o.process()
    assert np.array_equal(o.Out, np.arange(16, dtype=np.uint8) & 0xFF)


@case("midi: passthrough")
def _():
    o = h.load("avnd_test_midi")()
    o.In = [([0x90, 60, 100], 0), ([0x80, 60, 0], 5)]
    o.process()
    assert o.Out == [([144, 60, 100], 0), ([128, 60, 0], 5)]


@case("midi: note generation")
def _():
    o = h.load("avnd_test_midi_out")()
    o.Note = 64
    o.process()
    assert o.Out and o.Out[0][0][0] == 0x90 and o.Out[0][0][1] == 64


@case("dynamic port: sum")
def _():
    o = h.load("avnd_test_dynamic_port")()
    o.In = [1.0, 2.0, 3.0, 4.0]
    o.process()
    assert o.Sum == 10.0


@case("file port: bytes -> loaded")
def _():
    o = h.load("avnd_test_file")()
    o.In = b"hello world"
    o.process()
    assert o.Loaded is True


def main():
    if len(sys.argv) > 1:
        sys.path.insert(0, sys.argv[1])
    print("=== avendish Python backend harness ===")
    # (cases run at import via @case)
    print(f"\n{len(OK)} passed, {len(SKIP)} skipped, {len(FAIL)} failed")
    if FAIL:
        sys.exit(1)


if __name__ == "__main__":
    main()
