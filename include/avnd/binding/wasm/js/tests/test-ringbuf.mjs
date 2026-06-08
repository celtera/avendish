/* SPDX-License-Identifier: GPL-3.0-or-later */
// Unit tests for the SPSC RingBuffer. Run: node test-ringbuf.mjs

import { RingBuffer } from "../avnd-ringbuf.js";

let failures = 0;
function ok(cond, msg) {
  if (cond) {
    console.log("  ok  -", msg);
  } else {
    console.error("  FAIL-", msg);
    failures++;
  }
}
function eq(a, b, msg) {
  ok(a === b, `${msg} (got ${a}, want ${b})`);
}
function throws(fn, msg) {
  let threw = false;
  try {
    fn();
  } catch {
    threw = true;
  }
  ok(threw, msg);
}

// --- storageSize + power-of-two enforcement ---
console.log("storageSize / power-of-two:");
eq(RingBuffer.storageSize(16), 24, "storageSize(16) = 16 + 8");
eq(RingBuffer.storageSize(1024), 1032, "storageSize(1024)");
throws(() => RingBuffer.storageSize(17), "storageSize rejects non-power-of-two");
throws(() => RingBuffer.storageSize(0), "storageSize rejects 0");
throws(
  () => new RingBuffer(new ArrayBuffer(8 + 17)),
  "constructor rejects non-power-of-two storage",
);

// --- basic push/pop ---
console.log("basic push/pop:");
{
  const cap = 64;
  const rb = new RingBuffer(new ArrayBuffer(RingBuffer.storageSize(cap)));
  eq(rb.capacity, cap, "capacity");
  eq(rb.availableRead(), 0, "empty: availableRead 0");
  eq(rb.availableWrite(), cap, "empty: availableWrite cap");

  const src = new Uint8Array([1, 2, 3, 4, 5]);
  eq(rb.push(src), 5, "push 5 bytes");
  eq(rb.availableRead(), 5, "availableRead 5");
  eq(rb.availableWrite(), cap - 5, "availableWrite cap-5");

  const dst = new Uint8Array(5);
  eq(rb.pop(dst), 5, "pop 5 bytes");
  ok([...dst].every((v, i) => v === i + 1), "popped bytes match");
  eq(rb.availableRead(), 0, "empty again");
}

// --- fill exactly to capacity, then full ---
console.log("fill / full / empty:");
{
  const cap = 16;
  const rb = new RingBuffer(new ArrayBuffer(RingBuffer.storageSize(cap)));
  const full = new Uint8Array(cap).map((_, i) => i);
  eq(rb.push(full), cap, "push exactly cap bytes");
  eq(rb.availableWrite(), 0, "buffer full: availableWrite 0");
  eq(rb.push(new Uint8Array([99])), 0, "push on full returns 0");
  // partial push when near full
  const dst = new Uint8Array(cap);
  eq(rb.pop(dst), cap, "pop cap bytes");
  ok([...dst].every((v, i) => v === i), "full contents survive");
  eq(rb.pop(new Uint8Array(4)), 0, "pop on empty returns 0");
}

// --- partial push when not enough room ---
console.log("partial push:");
{
  const cap = 8;
  const rb = new RingBuffer(new ArrayBuffer(RingBuffer.storageSize(cap)));
  rb.push(new Uint8Array(6)); // 6 used, 2 free
  eq(rb.push(new Uint8Array([10, 11, 12, 13])), 2, "push clamps to free space (2)");
  eq(rb.availableWrite(), 0, "now full");
}

// --- wrap-around (write index passes the end of storage) ---
console.log("wrap-around bytes:");
{
  const cap = 8;
  const rb = new RingBuffer(new ArrayBuffer(RingBuffer.storageSize(cap)));
  // advance indices to 6, empty
  rb.push(new Uint8Array(6));
  rb.pop(new Uint8Array(6));
  eq(rb.availableRead(), 0, "drained, indices advanced to 6");
  // push 6 bytes: 2 fit before wrap, 4 wrap to start
  const payload = new Uint8Array([20, 21, 22, 23, 24, 25]);
  eq(rb.push(payload), 6, "push 6 spanning the wrap");
  const dst = new Uint8Array(6);
  eq(rb.pop(dst), 6, "pop 6 spanning the wrap");
  ok([...dst].every((v, i) => v === 20 + i), "wrapped bytes reassembled in order");
}

// --- many wraps via free-running indices ---
console.log("many wraps (free-running indices):");
{
  const cap = 32;
  const rb = new RingBuffer(new ArrayBuffer(RingBuffer.storageSize(cap)));
  let expected = 0;
  const chunk = 20; // not a divisor of cap -> exercises every wrap offset
  for (let iter = 0; iter < 1000; iter++) {
    const src = new Uint8Array(chunk);
    for (let i = 0; i < chunk; i++) src[i] = (expected + i) & 0xff;
    if (rb.push(src) !== chunk) {
      ok(false, `iter ${iter}: push`);
      break;
    }
    if (rb.availableRead() !== chunk) {
      ok(false, `iter ${iter}: availableRead`);
      break;
    }
    const dst = new Uint8Array(chunk);
    rb.pop(dst);
    let good = true;
    for (let i = 0; i < chunk; i++) if (dst[i] !== ((expected + i) & 0xff)) good = false;
    if (!good) {
      ok(false, `iter ${iter}: data mismatch after wrap`);
      break;
    }
    expected += chunk;
  }
  ok(true, `1000 push/pop cycles of ${chunk}B over cap=${cap} preserved data`);
}

// --- Float32 across a wrap boundary ---
console.log("Float32 across wrap:");
{
  const cap = 16; // 4 floats
  const rb = new RingBuffer(new ArrayBuffer(RingBuffer.storageSize(cap)));
  // move write offset to byte 14 so a float straddles the wrap
  rb.push(new Uint8Array(14));
  rb.pop(new Uint8Array(14));
  eq(rb.availableRead() | 0, 0, "drained to offset 14");

  const values = new Float32Array([3.14159, -2.71828, 1.41421]);
  const pushed = rb.pushFloats(values);
  eq(pushed, 3, "pushFloats 3 floats (12 bytes) straddling wrap at offset 14");
  const out = new Float32Array(3);
  const popped = rb.popFloats(out);
  eq(popped, 3, "popFloats 3 floats");
  let exact = true;
  for (let i = 0; i < 3; i++) if (out[i] !== values[i]) exact = false;
  ok(exact, `float values survive wrap bit-exact: [${Array.from(out)}]`);
}

// --- pushFloats is all-or-nothing ---
console.log("pushFloats all-or-nothing:");
{
  const cap = 16; // 4 floats
  const rb = new RingBuffer(new ArrayBuffer(RingBuffer.storageSize(cap)));
  rb.pushFloats(new Float32Array([1, 2, 3])); // 3 floats used, room for 1
  eq(
    rb.pushFloats(new Float32Array([4, 5])),
    0,
    "pushFloats rejects when it won't fully fit",
  );
  eq(rb.availableRead() >>> 2, 3, "still only 3 floats present");
}

// --- clear ---
console.log("clear:");
{
  const rb = new RingBuffer(new ArrayBuffer(RingBuffer.storageSize(32)));
  rb.push(new Uint8Array(10));
  rb.clear();
  eq(rb.availableRead(), 0, "clear empties readable bytes");
}

console.log("");
if (failures === 0) {
  console.log("RingBuffer: ALL TESTS PASSED");
} else {
  console.error(`RingBuffer: ${failures} FAILURE(S)`);
  process.exit(1);
}
