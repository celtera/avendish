#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run the WASM JS runtime test suite.
# Usage: run.sh [path-to-built-module.mjs]  (default: /tmp/avnd-p2/avnd_lowpass.mjs)
# audio-helper and node-runner tests need a built <c_name>.mjs + .wasm.

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE="${1:-/tmp/avnd-p2/avnd_lowpass.mjs}"

echo "== RingBuffer =="
node "$HERE/test-ringbuf.mjs"

echo
echo "== HeapAudioBuffer (module: $MODULE) =="
node "$HERE/test-audio-helper.mjs" "$MODULE"

echo
echo "== Node runner / DSP path (module: $MODULE) =="
node "$HERE/test-node-runner.mjs" "$MODULE"

echo
echo "ALL JS RUNTIME TESTS PASSED"
