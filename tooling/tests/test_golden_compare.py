#!/usr/bin/env python3
"""Unit tests for the shared golden comparison helpers.

    py -m unittest discover -s tooling/tests
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from golden_compare import compare_textures  # noqa: E402


def tex(w, h, hash_=1):
    return {"width": w, "height": h, "hash": hash_}


class CompareTextures(unittest.TestCase):
    def test_matching_dimensions_and_hash(self):
        verdict, _ = compare_textures([tex(16, 16)], [tex(16, 16)], 1e-3, 1e-3)
        self.assertEqual(verdict, "match")

    def test_wrong_size_is_a_mismatch(self):
        verdict, why = compare_textures([tex(8, 8)], [tex(16, 16)], 1e-3, 1e-3)
        self.assertEqual(verdict, "MISMATCH")
        self.assertIn("size", why)

    def test_content_hash_checked_in_hash_mode(self):
        verdict, _ = compare_textures(
            [tex(16, 16, 1)], [tex(16, 16, 2)], 1e-3, 1e-3, content="hash")
        self.assertEqual(verdict, "MISMATCH")

    def test_content_hash_informational_in_dims_mode(self):
        # Hosts that hand back a converted representation (Jitter's ARGB vs the
        # object's RGBA) can never reproduce the native byte hash.
        verdict, _ = compare_textures(
            [tex(16, 16, 1)], [tex(16, 16, 2)], 1e-3, 1e-3, content="dims")
        self.assertEqual(verdict, "match")

    def test_degenerate_golden_texture_is_not_a_failure(self):
        # A filter downscaling 16x16 by 32 yields a 0x0 image. No host matrix
        # type can represent that -- Jitter has no 0x0 matrix -- so the backend
        # necessarily reports something else and there is nothing to diff.
        verdict, why = compare_textures(
            [tex(16, 16)], [tex(0, 0)], 1e-3, 1e-3, content="dims")
        self.assertEqual(verdict, "no-golden-texture")
        self.assertIn("degenerate", why)

    def test_degenerate_entry_does_not_mask_a_real_one(self):
        # One degenerate port must not excuse a genuinely wrong sibling.
        verdict, _ = compare_textures(
            [tex(0, 0), tex(8, 8)], [tex(0, 0), tex(16, 16)], 1e-3, 1e-3,
            content="dims")
        self.assertEqual(verdict, "MISMATCH")

        verdict, why = compare_textures(
            [tex(0, 0), tex(16, 16)], [tex(0, 0), tex(16, 16)], 1e-3, 1e-3,
            content="dims")
        self.assertEqual(verdict, "match")
        self.assertIn("degenerate", why)

    def test_no_texture_either_side(self):
        self.assertEqual(compare_textures([], [tex(1, 1)], 1e-3, 1e-3)[0],
                         "no-backend-texture")
        self.assertEqual(compare_textures([tex(1, 1)], [], 1e-3, 1e-3)[0],
                         "no-golden-texture")


if __name__ == "__main__":
    unittest.main()
