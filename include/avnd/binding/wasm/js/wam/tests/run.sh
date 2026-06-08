#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run the WAM2 packaging test suite: param mapping + node logic, node --check on
# every WAM js (placeholders substituted), and descriptor/package.json validity.
# Usage: run.sh [modules-dir]  (default /tmp/avnd-wasm-test)

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WAM="$(cd "$HERE/.." && pwd)"
MODULES_DIR="${1:-/tmp/avnd-wasm-test}"

echo "== WAM parameter mapping + node logic =="
node "$HERE/test-wam-params.mjs" "$MODULES_DIR"

echo
echo "== node --check every WAM js (placeholders substituted) =="
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

subst() {
  # Replace @AVND_...@ placeholders with test-safe values.
  sed \
    -e 's/@AVND_WASM_C_NAME@/avnd_test/g' \
    -e 's/@AVND_WASM_NAME@/Avnd Test/g' \
    -e 's/@AVND_WASM_VENDOR@/Avendish/g' \
    -e 's#@AVND_WASM_VENDOR_URL@#https://celtera.dev#g' \
    -e 's/@AVND_WASM_VERSION@/1.0.0/g' \
    "$1"
}

for f in "$WAM"/*.js "$WAM"/*.js.in; do
  [ -e "$f" ] || continue
  base="$(basename "$f")"
  out="$TMP/${base%.in}"
  # ensure .js extension so node treats it as a module
  case "$out" in
    *.js) : ;;
    *) out="$out.js" ;;
  esac
  subst "$f" > "$out"
  if node --check "$out"; then
    echo "  ok   $base"
  else
    echo "  FAIL $base"
    exit 1
  fi
done

echo
echo "== descriptor.json valid JSON after substitution =="
subst "$WAM/descriptor.json.in" > "$TMP/descriptor.json"
node -e 'const d=require(process.argv[1]); if(!d.name||d.apiVersion!=="2.0.0") throw new Error("bad descriptor"); console.log("  ok   descriptor.json ("+d.name+")")' "$TMP/descriptor.json"

subst "$WAM/package.json.in" > "$TMP/package.json"
node -e 'const p=require(process.argv[1]); if(p.type!=="module"||!p.exports["."]) throw new Error("bad package.json"); console.log("  ok   package.json")' "$TMP/package.json"

echo
echo "ALL WAM TESTS PASSED"
