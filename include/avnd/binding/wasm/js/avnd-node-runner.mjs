/* SPDX-License-Identifier: GPL-3.0-or-later */

// Headless offline runner for an Avendish WASM module. Runs under Node/Deno/Bun.
// See README for usage.

import { AvndProcessor } from "./avnd-dsp.js";

// e.g. ".../avnd_lowpass.mjs" -> "avnd_lowpass".
function defaultCName(modulePath) {
  const base = modulePath.split(/[\\/]/).pop() || modulePath;
  return base.replace(/\.mjs$/, "").replace(/\.js$/, "");
}

// Load a module and build a runner. opts: { cName, in, out, rate, frames }.
export async function render(modulePath, opts = {}) {
  const cName = opts.cName ?? defaultCName(modulePath);
  const rate = opts.rate ?? 48000;
  const frames = opts.frames ?? 256;
  const reqIn = opts.in ?? 2;
  const reqOut = opts.out ?? 2;

  const url = await resolveModuleUrl(modulePath);
  const ns = await import(url);
  const factory = ns.default;
  if (typeof factory !== "function") {
    throw new Error(`Module ${modulePath} has no default Emscripten factory export`);
  }
  const M = await factory();

  const proc = new AvndProcessor(M, cName, {
    blockSize: frames,
    sampleRate: rate,
    requestedIn: reqIn,
    requestedOut: reqOut,
  });

  return new Runner(M, proc);
}

async function resolveModuleUrl(modulePath) {
  if (/^[a-z]+:\/\//i.test(modulePath)) return modulePath;
  try {
    const { pathToFileURL } = await import("node:url");
    const { resolve, isAbsolute } = await import("node:path");
    const abs = isAbsolute(modulePath) ? modulePath : resolve(process.cwd(), modulePath);
    return pathToFileURL(abs).href;
  } catch {
    return modulePath; // Deno without node compat: relative specifiers work
  }
}

// Stable public wrapper around AvndProcessor for offline rendering.
export class Runner {
  constructor(M, proc) {
    this.M = M;
    this._proc = proc;
  }

  get inChannels() {
    return this._proc.inChannels;
  }
  get outChannels() {
    return this._proc.outChannels;
  }
  get frames() {
    return this._proc.blockSize;
  }
  get sampleRate() {
    return this._proc.sampleRate;
  }
  get name() {
    return this._proc.name;
  }

  /** Process one block of planar input -> fresh planar output. */
  processBlock(inputPlanes) {
    return this._proc.processToNew(inputPlanes ?? []);
  }

  /** Render an arbitrary-length planar signal by chunking into blocks (tail zero-padded). */
  renderToBuffer(inputPlanes, blockSize = this.frames) {
    if (blockSize !== this.frames) {
      throw new Error(
        `renderToBuffer: blockSize ${blockSize} must equal the prepared block ` +
          `size ${this.frames} (Phase 2 does not yet adapt block size)`,
      );
    }
    const inCh = inputPlanes.length;
    const total = inCh > 0 ? inputPlanes[0].length : 0;
    const outCh = this.outChannels;
    const out = [];
    for (let c = 0; c < outCh; c++) out.push(new Float32Array(total));

    const inBlock = [];
    for (let c = 0; c < inCh; c++) inBlock.push(new Float32Array(blockSize));

    for (let pos = 0; pos < total; pos += blockSize) {
      const n = Math.min(blockSize, total - pos);
      for (let c = 0; c < inCh; c++) {
        inBlock[c].fill(0);
        inBlock[c].set(inputPlanes[c].subarray(pos, pos + n));
      }
      const outBlock = this.processBlock(inBlock);
      for (let c = 0; c < outCh; c++) {
        out[c].set(outBlock[c].subarray(0, n), pos);
      }
    }
    return out;
  }

  // --- parameter passthrough ---
  getParameterCount() {
    return this._proc.getParameterCount();
  }
  getParameterInfo(i) {
    return this._proc.getParameterInfo(i);
  }
  setParameter(i, v) {
    this._proc.setParameter(i, v);
  }
  getParameter(i) {
    return this._proc.getParameter(i);
  }
  setParameterNormalized(i, v) {
    this._proc.setParameterNormalized(i, v);
  }
  getParameterNormalized(i) {
    return this._proc.getParameterNormalized(i);
  }
  getOutputCount() {
    return this._proc.getOutputCount();
  }
  getOutputInfo(i) {
    return this._proc.getOutputInfo(i);
  }
  getOutputValue(i) {
    return this._proc.getOutputValue(i);
  }

  destroy() {
    this._proc.destroy();
  }
}

// --- CLI: renders a unit impulse and prints per-channel stats ---

function parseArgs(argv) {
  const args = { _: [] };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a.startsWith("--")) {
      const key = a.slice(2);
      const next = argv[i + 1];
      if (next === undefined || next.startsWith("--")) {
        args[key] = true;
      } else {
        args[key] = next;
        i++;
      }
    } else {
      args._.push(a);
    }
  }
  return args;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const modulePath = args._[0];
  if (!modulePath) {
    console.error(
      "usage: node avnd-node-runner.mjs <module.mjs> [--cname NAME] " +
        "[--in N] [--out N] [--rate R] [--frames F]",
    );
    process.exit(2);
  }
  const r = await render(modulePath, {
    cName: args.cname,
    in: args.in ? +args.in : undefined,
    out: args.out ? +args.out : undefined,
    rate: args.rate ? +args.rate : undefined,
    frames: args.frames ? +args.frames : undefined,
  });

  console.log(`plug-in : ${r.name}`);
  console.log(`channels: in=${r.inChannels} out=${r.outChannels}`);
  console.log(`block   : ${r.frames} @ ${r.sampleRate} Hz`);
  const pc = r.getParameterCount();
  console.log(`params  : ${pc}`);
  for (let i = 0; i < pc; i++) {
    console.log("  -", JSON.stringify(r.getParameterInfo(i)));
  }

  // Unit impulse on every input channel.
  const ins = [];
  for (let c = 0; c < r.inChannels; c++) {
    const ch = new Float32Array(r.frames);
    ch[0] = 1.0;
    ins.push(ch);
  }

  const outs = r.processBlock(ins);
  console.log(`rendered: ${outs.length} output plane(s) x ${r.frames} frames`);
  outs.forEach((ch, c) => {
    let peak = 0;
    let energy = 0;
    for (let i = 0; i < ch.length; i++) {
      const v = Math.abs(ch[i]);
      if (v > peak) peak = v;
      energy += ch[i] * ch[i];
    }
    const rms = Math.sqrt(energy / ch.length);
    console.log(
      `  ch${c}: peak=${peak.toFixed(6)} rms=${rms.toFixed(6)} first8=[${Array.from(
        ch.subarray(0, 8),
      )
        .map((x) => x.toFixed(4))
        .join(", ")}]`,
    );
  });

  r.destroy();
}

// Run main() only when executed directly, not when imported.
const isMain =
  typeof process !== "undefined" &&
  process.argv[1] &&
  import.meta.url === `file://${process.argv[1]}` ||
  (typeof process !== "undefined" &&
    process.argv[1] &&
    import.meta.url.endsWith(process.argv[1].replace(/\\/g, "/")));

if (isMain) {
  main().catch((e) => {
    console.error(e);
    process.exit(1);
  });
}

export default { render, Runner };
