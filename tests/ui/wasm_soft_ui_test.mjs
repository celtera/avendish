// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless node test for the soft-framebuffer WASM shell: render frames,
// check they are non-blank and deterministic, and drive the custom widget
// with pointer events (the drag must change the rendered output — the
// widget's fill bar moves).
//
//   node wasm_soft_ui_test.mjs ./wasm_soft_ui.mjs

import { strict as assert } from "node:assert";

const modulePath = process.argv[2] ?? "./wasm_soft_ui.mjs";
const factory = (await import(new URL(modulePath, `file://${process.cwd()}/`).href))
  .default;
const Module = await factory();

const ui = new Module.SoftUI();
ui.resize(320, 220, 1.0);
assert.equal(ui.physicalWidth(), 320);
assert.equal(ui.physicalHeight(), 220);

const snapshot = () => {
  const px = ui.frame();
  return px ? Uint8ClampedArray.from(px) : null;
};

const first = Uint8ClampedArray.from(ui.renderNow());
assert.equal(first.length, 320 * 220 * 4);
const nonzero = first.reduce((n, v) => n + (v !== 0 ? 1 : 0), 0);
assert.ok(nonzero > 10000, `blank frame? nonzero=${nonzero}`);

// Determinism: a forced re-render must be byte-identical...
const forced = Uint8ClampedArray.from(ui.renderNow());
assert.deepEqual(forced, first, "render must be deterministic");
// ...and a clean frame() reports "unchanged" (dirty tracking).
assert.equal(snapshot(), null, "clean frame must skip rendering");

// Drag inside the 200x100 custom widget near the top-left: the orange fill
// follows the pointer, so the frame must change (and must not be null).
ui.pointerMove(30, 50);
ui.pointerButton(30, 50, true);
snapshot(); // deliver press
ui.pointerMove(180, 50);
snapshot(); // drag
ui.pointerButton(180, 50, false);
const after = snapshot();
assert.ok(after, "interaction must produce a dirty frame");

let diff = 0;
for (let i = 0; i < after.length; i++) if (after[i] !== first[i]) diff++;
assert.ok(diff > 1000, `interaction did not change the frame (diff=${diff})`);

console.log(
  `wasm soft ui OK: ${nonzero} nonzero bytes, deterministic, clean frames skipped, drag changed ${diff} bytes`,
);
