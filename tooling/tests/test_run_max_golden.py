#!/usr/bin/env python3
"""Unit tests for the Max golden harness' report handling.

The harness once blamed two objects for crashing Max when they had merely
produced report lines too long for Max's js File.writeline (which truncates at
32767 characters rather than splitting). An unparseable entry meant the object
never counted as done, the stall timer fired, and whatever object was current
got stamped as the crasher. These tests pin the parsing side of that.

    py -m unittest discover -s tooling/tests
"""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import run_max_golden as G  # noqa: E402


def write(tmp, text):
    path = os.path.join(tmp, "report.jsonl")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    return path


class ParseReport(unittest.TestCase):
    def test_one_entry_per_line(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = write(tmp, '{"name":"a","cases":[]}\n{"name":"b","cases":[]}\n')
            got = G.parse_report(p)
        self.assertEqual(sorted(got), ["a", "b"])

    def test_entry_split_across_physical_lines_is_rejoined(self):
        # What Max produces when its write buffer flushes mid-entry: the entry
        # arrives as several physical lines and only the first carries "name".
        payload = {"name": "big", "cases": [{"audio": [[0.5] * 4000]}]}
        blob = json.dumps(payload)
        third = len(blob) // 3
        split = blob[:third] + "\n" + blob[third:2 * third] + "\n" + blob[2 * third:]
        with tempfile.TemporaryDirectory() as tmp:
            p = write(tmp, split + "\n")
            got = G.parse_report(p)
        self.assertIn("big", got)
        self.assertEqual(got["big"], payload)

    def test_split_entry_followed_by_a_normal_one(self):
        big = json.dumps({"name": "big", "cases": [{"audio": [[1.0] * 2000]}]})
        half = len(big) // 2
        text = big[:half] + "\n" + big[half:] + "\n" + '{"name":"small","cases":[]}\n'
        with tempfile.TemporaryDirectory() as tmp:
            got = G.parse_report(write(tmp, text))
        self.assertEqual(sorted(got), ["big", "small"])

    def test_a_corrupt_entry_does_not_swallow_the_rest(self):
        # A truncated entry must not consume the objects that follow it --
        # otherwise one over-long line loses every later result.
        text = ('{"name":"truncated","cases":[{"audio":[[0.1,0.2\n'
                '{"name":"good","cases":[]}\n')
        with tempfile.TemporaryDirectory() as tmp:
            got = G.parse_report(write(tmp, text))
        self.assertIn("good", got)
        self.assertNotIn("truncated", got)

    def test_missing_file_is_empty_not_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(G.parse_report(os.path.join(tmp, "nope.jsonl")), {})

    def test_blank_lines_are_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            got = G.parse_report(write(tmp, '\n{"name":"a","cases":[]}\n\n'))
        self.assertEqual(sorted(got), ["a"])


class MaxSymbol(unittest.TestCase):
    """The selector the Max binding matches a port name against."""

    def test_matches_fixup_identifier(self):
        self.assertEqual(G.max_symbol("Pre-smooth"), "Pre_smooth")
        self.assertEqual(G.max_symbol("Min size"), "Min_size")
        self.assertEqual(G.max_symbol("already.ok~"), "already.ok~")
        self.assertEqual(G.max_symbol("a/b:c"), "a_b_c")


class CapLayout(unittest.TestCase):
    """Which outlet carries which capturable output.

    For an audio object the non-audio ports sit to the RIGHT of the single
    signal outlet, so capture starts at 1 -- unless the object has no audio
    output at all (an analyzer), where they start at 0.
    """

    def test_message_object_starts_at_zero(self):
        outs = [{"type": "parameter", "name": "a"}, {"type": "callback", "name": "b"}]
        base, caps = G.cap_layout(outs, [{}], "control")
        self.assertEqual(base, 0)
        self.assertEqual(caps, [["parameter", "a"], ["callback", "b"]])

    def test_audio_object_skips_the_signal_outlet(self):
        outs = [{"type": "audio", "name": "out"}, {"type": "parameter", "name": "peak"}]
        base, caps = G.cap_layout(outs, [{}], "audio")
        self.assertEqual(base, 1)
        self.assertEqual(caps, [["parameter", "peak"]])

    def test_analyzer_without_audio_output_starts_at_zero(self):
        outs = [{"type": "parameter", "name": "peak"}]
        base, caps = G.cap_layout(outs, [{}], "audio")
        self.assertEqual(base, 0)
        self.assertEqual(caps, [["parameter", "peak"]])


class ObjectKind(unittest.TestCase):
    """Which driver builds an object -- and therefore what it is fed."""

    @staticmethod
    def kind_of(golden):
        with tempfile.TemporaryDirectory() as tmp:
            objs = G.gen_data_js([golden], os.path.join(tmp, "data.js"), None)
        return objs[0]["kind"]

    def test_texture_filter(self):
        g = {"c_name": "f", "_stem": "f", "cases": [{
            "inputs": {"texture": [{"width": 16, "height": 16}]},
            "outputs": {"texture": [{"width": 16, "height": 16}]}}]}
        self.assertEqual(self.kind_of(g), "texture")

    def test_texture_analyzer_is_a_matrix_operator_not_a_control_object(self):
        # The regression: texture IN, control OUT, no texture out. Classified
        # "control" it was built by buildControl, which never sends a
        # jit_matrix -- so it analyzed an image it had never been given.
        g = {"c_name": "a", "_stem": "a", "cases": [{
            "inputs": {"texture": [{"width": 16, "height": 16}]},
            "outputs": {"controls": [{"index": 0, "name": "Width", "value": 16}]}}]}
        self.assertEqual(self.kind_of(g), "texture")

    def test_plain_control_object(self):
        g = {"c_name": "c", "_stem": "c", "cases": [{
            "inputs": {"controls": [{"index": 0, "name": "In", "value": 1}]},
            "outputs": {"controls": [{"index": 0, "name": "Out", "value": 1}]}}]}
        self.assertEqual(self.kind_of(g), "control")

    def test_audio_wins_over_a_matrix_port(self):
        # prototype.cpp.in tests the audio branch first, so an object with both
        # is a DSP object, not a matrix operator.
        g = {"c_name": "m", "_stem": "m", "cases": [{
            "inputs": {"audio": [[0.0]], "texture": [{"width": 4, "height": 4}]},
            "outputs": {"audio": [[0.0]]}}]}
        self.assertEqual(self.kind_of(g), "audio")


class Stall(unittest.TestCase):
    """The stall detector that used to blame slow objects for crashing Max."""

    def test_nothing_counts_before_the_first_breadcrumb(self):
        # Max's own 30-45s startup must not burn the stall budget.
        t = G.StallTracker(0.0, stall_seconds=60)
        t.update(50.0, "", 0)
        self.assertFalse(t.started)
        self.assertFalse(t.stalled(1000.0))

    def test_a_moving_breadcrumb_is_progress_even_with_no_report_entries(self):
        # The regression: an object whose report line is too long to write in
        # one piece never yields a parseable entry, so counting entries alone
        # looked like a stall while the driver was working fine.
        t = G.StallTracker(0.0, stall_seconds=60)
        now = 1.0
        for i in range(20):
            t.update(now, "obj%d:case0" % i, 0)
            now += 30.0
            self.assertFalse(t.stalled(now), "stalled at object %d" % i)

    def test_a_frozen_breadcrumb_eventually_stalls(self):
        t = G.StallTracker(0.0, stall_seconds=60)
        t.update(1.0, "stuck:case0", 0)
        self.assertFalse(t.stalled(30.0))
        self.assertTrue(t.stalled(120.0))

    def test_new_report_entries_are_progress_too(self):
        t = G.StallTracker(0.0, stall_seconds=60)
        t.update(1.0, "a:case0", 0)
        t.update(100.0, "a:case0", 1)   # same crumb, but an entry landed
        self.assertFalse(t.stalled(120.0))
        self.assertTrue(t.stalled(200.0))


if __name__ == "__main__":
    unittest.main()
