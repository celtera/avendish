/* SPDX-License-Identifier: GPL-3.0-or-later */

// AudioContext-free DSP marshaling shared by the worklet and the Node runner,
// so the hot path is unit-testable in plain Node.

import { HeapAudioBuffer } from "./avnd-audio-helper.js";

export class AvndProcessor {
  constructor(M, cName, opts) {
    const Cls = M[cName];
    if (!Cls) {
      throw new Error(`Embind class "${cName}" not found on the module`);
    }
    this.M = M;
    this.cName = cName;
    this.blockSize = opts.blockSize;
    this.sampleRate = opts.sampleRate;
    const reqIn = opts.requestedIn ?? 2;
    const reqOut = opts.requestedOut ?? 2;

    this.plugin = new Cls();
    this.plugin.prepare(reqIn, reqOut, this.blockSize, this.sampleRate);

    // Plug-in may negotiate different channel counts.
    this.inChannels = this.plugin.inputChannels();
    this.outChannels = this.plugin.outputChannels();

    // Always allocate >=1 channel so .ptr is valid even with 0 input channels.
    this.inBuf = new HeapAudioBuffer(
      M,
      this.blockSize,
      Math.max(1, this.inChannels),
    );
    this.outBuf = new HeapAudioBuffer(
      M,
      this.blockSize,
      Math.max(1, this.outChannels),
    );
  }

  get name() {
    return this.M[this.cName].getName();
  }

  /** Process exactly `blockSize` frames. */
  process(inputPlanes, outputPlanes) {
    this.inBuf.copyFromAudioWorklet(inputPlanes);
    this.plugin.process(this.inBuf.ptr, this.outBuf.ptr, this.blockSize);
    this.outBuf.copyToAudioWorklet(outputPlanes);
  }

  /** Allocate and return fresh output planes for one block. */
  processToNew(inputPlanes) {
    const out = [];
    for (let c = 0; c < this.outChannels; c++) {
      out.push(new Float32Array(this.blockSize));
    }
    this.process(inputPlanes, out);
    return out;
  }

  // --- parameter / output passthrough -----------------------------------

  getParameterCount() {
    return this.plugin.getParameterCount();
  }
  getParameterInfo(i) {
    return this.plugin.getParameterInfo(i);
  }
  setParameter(i, v) {
    this.plugin.setParameter(i, v);
  }
  getParameter(i) {
    return this.plugin.getParameter(i);
  }
  setParameterNormalized(i, v) {
    this.plugin.setParameterNormalized(i, v);
  }
  getParameterNormalized(i) {
    return this.plugin.getParameterNormalized(i);
  }
  getOutputCount() {
    return this.plugin.getOutputCount();
  }
  getOutputInfo(i) {
    return this.plugin.getOutputInfo(i);
  }
  getOutputValue(i) {
    return this.plugin.getOutputValue(i);
  }

  // --- message / callback passthrough -----------------------------------

  // Returns true if the message was dispatched.
  sendMessage(name, args) {
    if (typeof this.plugin.sendMessage !== "function") return false;
    return this.plugin.sendMessage(name, args || []);
  }

  getCallbackCount() {
    return typeof this.plugin.getCallbackCount === "function"
      ? this.plugin.getCallbackCount()
      : 0;
  }
  getCallbackInfo(i) {
    return this.plugin.getCallbackInfo(i);
  }

  // Drain emitted callbacks into plain JS {index,name,args} so they're
  // structured-cloneable for postMessage.
  drainCallbacks() {
    if (typeof this.plugin.drainCallbacks !== "function") return [];
    let events;
    try {
      events = this.plugin.drainCallbacks();
    } catch {
      return [];
    }
    if (!events) return [];
    const out = [];
    const n = events.length || 0;
    for (let i = 0; i < n; i++) {
      const e = events[i];
      if (!e) continue;
      const raw = e.args;
      const args = [];
      if (raw) {
        const m = raw.length || 0;
        for (let j = 0; j < m; j++) args.push(raw[j]);
      }
      out.push({ index: e.index, name: e.name, args });
    }
    return out;
  }

  /** Release heap buffers and the Embind instance. */
  destroy() {
    if (this.inBuf) this.inBuf.free();
    if (this.outBuf) this.outBuf.free();
    if (this.plugin) this.plugin.delete();
    this.inBuf = this.outBuf = this.plugin = null;
  }
}

export default AvndProcessor;
