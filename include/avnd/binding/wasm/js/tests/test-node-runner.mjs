/* SPDX-License-Identifier: GPL-3.0-or-later */
// Integration test: render an impulse through Lowpass via the node runner.
// Run from the build dir:  node test-node-runner.mjs ./avnd_lowpass.mjs

import { render } from "../avnd-node-runner.mjs";

const modulePath = process.argv[2] ?? "./avnd_lowpass.mjs";

let failures = 0;
function ok(cond, msg) {
  console[cond ? "log" : "error"](`  ${cond ? "ok  " : "FAIL"}- ${msg}`);
  if (!cond) failures++;
}

const r = await render(modulePath, {
  cName: "avnd_lowpass",
  in: 2,
  out: 2,
  rate: 48000,
  frames: 256,
});

console.log(`plug-in: ${r.name}  in=${r.inChannels} out=${r.outChannels} frames=${r.frames}`);

ok(r.name === "Lowpass", "metadata name is Lowpass");
ok(r.inChannels === 2, "negotiated 2 input channels");
ok(r.outChannels === 2, "negotiated 2 output channels");
ok(r.getParameterCount() === 1, "one parameter (Weight)");

const info = r.getParameterInfo(0);
ok(info.identifier === "Weight", "param 0 is Weight");
ok(info.type === "float", "param 0 type float");
ok(info.min === 0 && info.max === 1, "param 0 range [0,1]");

// weight = 1 -> output should equal input (out = w*in + (1-w)*prev = in)
r.setParameter(0, 1.0);
ok(Math.abs(r.getParameter(0) - 1.0) < 1e-6, "setParameter/getParameter round-trips");

function impulse() {
  const a = new Float32Array(r.frames);
  a[0] = 1.0;
  const b = new Float32Array(r.frames);
  b[0] = 1.0;
  return [a, b];
}

let outs = r.processBlock(impulse());
ok(outs.length === r.outChannels, "output plane count == outChannels");
ok(outs[0].length === r.frames, "output plane length == frames");

// With weight=1, first sample passes through (=1), rest 0.
ok(Math.abs(outs[0][0] - 1.0) < 1e-5, "weight=1: impulse passes through (ch0[0]==1)");
ok(Math.abs(outs[1][0] - 1.0) < 1e-5, "weight=1: impulse passes through (ch1[0]==1)");

// Non-silent assertion.
function energy(planes) {
  let e = 0;
  for (const p of planes) for (let i = 0; i < p.length; i++) e += p[i] * p[i];
  return e;
}
ok(energy(outs) > 0, "output is non-silent");

// weight=0.5: impulse smears into a decaying tail. Fresh runner for clean state.
const r2 = await render(modulePath, { cName: "avnd_lowpass", in: 1, out: 1, frames: 256 });
r2.setParameter(0, 0.5);
const single = [new Float32Array(r2.frames)];
single[0][0] = 1.0;
const o2 = r2.processBlock(single);
ok(o2[0][0] > 0 && o2[0][1] > 0 && o2[0][2] > 0, "weight=0.5: impulse smears (decaying tail)");
ok(o2[0][1] < o2[0][0], "tail decays (sample1 < sample0)");

// renderToBuffer over a multi-block step, length not a multiple of block size.
const N = r.frames * 3 + 17;
const stepIn = [new Float32Array(N).fill(1.0), new Float32Array(N).fill(1.0)];
const r3 = await render(modulePath, { cName: "avnd_lowpass", in: 2, out: 2, frames: r.frames });
r3.setParameter(0, 1.0);
const big = r3.renderToBuffer(stepIn, r3.frames);
ok(big.length === 2, "renderToBuffer: 2 output planes");
ok(big[0].length === N, `renderToBuffer: output length == input length (${N})`);
ok(Math.abs(big[0][N - 1] - 1.0) < 1e-5, "renderToBuffer: steady-state step output == 1");

// channel-count mismatch: mono input into a 2ch plug-in must zero-fill, not crash.
const mono = [new Float32Array(r.frames)];
mono[0][0] = 1.0;
const mixed = r.processBlock(mono);
ok(mixed.length === 2 && mixed[0].length === r.frames, "mono input -> 2 output planes, no crash");

r.destroy();
r2.destroy();
r3.destroy();

console.log("");
if (failures === 0) {
  console.log("node runner / DSP path: ALL TESTS PASSED");
} else {
  console.error(`node runner / DSP path: ${failures} FAILURE(S)`);
  process.exit(1);
}
