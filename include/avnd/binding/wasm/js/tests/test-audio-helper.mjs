/* SPDX-License-Identifier: GPL-3.0-or-later */
// Tests for HeapAudioBuffer. Needs a built module for a real heap.
// Run:  node test-audio-helper.mjs /tmp/avnd-p2/avnd_lowpass.mjs

import { readFile } from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { HeapAudioBuffer } from "../avnd-audio-helper.js";

const modulePath = process.argv[2] ?? "./avnd_lowpass.mjs";

let failures = 0;
function ok(cond, msg) {
  console[cond ? "log" : "error"](`  ${cond ? "ok  " : "FAIL"}- ${msg}`);
  if (!cond) failures++;
}

const ns = await import(pathToFileURL(modulePath).href);
const M = await ns.default();

// --- planar layout: channel c at float index ptr/4 + c*frames ---
{
  const frames = 16;
  const ch = 3;
  const buf = new HeapAudioBuffer(M, frames, ch);
  for (let c = 0; c < ch; c++) {
    const data = buf.getChannelData(c);
    ok(data.length === frames, `getChannelData(${c}) length == frames`);
    for (let i = 0; i < frames; i++) data[i] = c * 100 + i;
  }
  const base = buf.ptr >>> 2;
  let layoutOk = true;
  for (let c = 0; c < ch; c++)
    for (let i = 0; i < frames; i++)
      if (M.HEAPF32[base + c * frames + i] !== c * 100 + i) layoutOk = false;
  ok(layoutOk, "planar layout: channel c starts at ptr/4 + c*frames");
  buf.free();
}

// --- interleaved round-trip ---
{
  const frames = 8;
  const ch = 2;
  const buf = new HeapAudioBuffer(M, frames, ch);
  const inter = new Float32Array(frames * ch);
  for (let i = 0; i < frames; i++) {
    inter[i * ch + 0] = i; // L
    inter[i * ch + 1] = -i; // R
  }
  buf.fromInterleaved(inter, ch);
  ok(buf.getChannelData(0)[3] === 3 && buf.getChannelData(1)[3] === -3, "fromInterleaved deinterleaves");
  const back = buf.toInterleaved();
  let same = true;
  for (let i = 0; i < inter.length; i++) if (back[i] !== inter[i]) same = false;
  ok(same, "toInterleaved is the inverse of fromInterleaved");
  buf.free();
}

// --- copyFromAudioWorklet: fewer provided channels -> zero-fill the rest ---
{
  const frames = 4;
  const buf = new HeapAudioBuffer(M, frames, 2); // heap wants 2
  const workletIn = [new Float32Array([1, 2, 3, 4])]; // only 1 provided
  buf.copyFromAudioWorklet(workletIn);
  ok([...buf.getChannelData(0)].join() === "1,2,3,4", "ch0 copied from worklet input");
  ok([...buf.getChannelData(1)].every((v) => v === 0), "missing input ch1 zero-filled");
  buf.free();
}

// --- copyFromAudioWorklet: more provided channels -> extras dropped ---
{
  const frames = 4;
  const buf = new HeapAudioBuffer(M, frames, 1); // heap wants 1
  const workletIn = [new Float32Array([5, 6, 7, 8]), new Float32Array([9, 9, 9, 9])];
  buf.copyFromAudioWorklet(workletIn);
  ok([...buf.getChannelData(0)].join() === "5,6,7,8", "extra worklet input channel dropped");
  buf.free();
}

// --- copyToAudioWorklet: more output channels than heap -> surplus silenced ---
{
  const frames = 4;
  const buf = new HeapAudioBuffer(M, frames, 1);
  buf.getChannelData(0).set([10, 20, 30, 40]);
  const outBus = [new Float32Array(frames), new Float32Array(frames).fill(7)];
  buf.copyToAudioWorklet(outBus);
  ok([...outBus[0]].join() === "10,20,30,40", "heap ch0 -> output ch0");
  ok([...outBus[1]].every((v) => v === 0), "surplus output channel silenced");
  buf.free();
}

// --- detached-buffer resilience: force memory growth, views still valid ---
{
  const frames = 64;
  const buf = new HeapAudioBuffer(M, frames, 2);
  buf.getChannelData(0)[0] = 42;
  const oldBuffer = M.HEAPF32.buffer;
  // Large alloc to (likely) trigger growth + detach the old ArrayBuffer.
  const big = M._malloc(64 * 1024 * 1024);
  const grew = M.HEAPF32.buffer !== oldBuffer;
  let valueOk = false;
  try {
    valueOk = buf.getChannelData(0)[0] === 42;
  } catch {
    valueOk = false;
  }
  ok(valueOk, `getChannelData survives heap growth (grew=${grew})`);
  if (big) M._free(big);
  buf.free();
}

console.log("");
if (failures === 0) {
  console.log("HeapAudioBuffer: ALL TESTS PASSED");
} else {
  console.error(`HeapAudioBuffer: ${failures} FAILURE(S)`);
  process.exit(1);
}
