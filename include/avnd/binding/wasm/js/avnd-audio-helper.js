/* SPDX-License-Identifier: GPL-3.0-or-later */

// Planar float32 audio buffer in the Emscripten WASM heap, for plugin.process(inPtr, outPtr, frames).
// Channel c is at float index ptr/4 + c*frames, frames contiguous samples each.
// Under ALLOW_MEMORY_GROWTH the heap buffer can be detached on _malloc, so we
// never cache views: every accessor re-derives from the current HEAPF32. .ptr is stable.
export class HeapAudioBuffer {
  constructor(module, frames, channels) {
    this._m = module;
    this._frames = frames;
    this._channels = channels;
    this._floatsPerChannel = frames;
    this._totalFloats = frames * channels;
    this._byteLength = this._totalFloats * 4;
    this._ptr = module._malloc(this._byteLength);
    if (this._ptr === 0) {
      throw new Error(
        `HeapAudioBuffer: _malloc(${this._byteLength}) failed (out of WASM memory)`,
      );
    }
    this.clear();
  }

  /** Byte offset into the WASM heap (pass this to plugin.process). */
  get ptr() {
    return this._ptr;
  }

  get frames() {
    return this._frames;
  }

  get channels() {
    return this._channels;
  }

  /** Fresh view over the whole buffer (re-derived for growth safety). */
  _all() {
    const base = this._ptr >>> 2;
    return this._m.HEAPF32.subarray(base, base + this._totalFloats);
  }

  /** Fresh view over channel `c` (length = frames). */
  getChannelData(c) {
    if (c < 0 || c >= this._channels) {
      throw new RangeError(`channel ${c} out of range [0, ${this._channels})`);
    }
    const base = (this._ptr >>> 2) + c * this._frames;
    return this._m.HEAPF32.subarray(base, base + this._frames);
  }

  /** Zero the whole buffer. */
  clear() {
    this._all().fill(0);
  }

  // --- interleaved <-> planar -------------------------------------------

  /** Fill from interleaved; extra src channels ignored, missing ones zeroed. */
  fromInterleaved(interleaved, srcChannels) {
    const all = this._all();
    const frames = this._frames;
    const ch = this._channels;
    for (let c = 0; c < ch; c++) {
      const dstBase = c * frames;
      if (c < srcChannels) {
        for (let i = 0; i < frames; i++) {
          all[dstBase + i] = interleaved[i * srcChannels + c];
        }
      } else {
        for (let i = 0; i < frames; i++) all[dstBase + i] = 0;
      }
    }
  }

  /** Read out into an interleaved Float32Array (allocated if `out` omitted). */
  toInterleaved(out) {
    const frames = this._frames;
    const ch = this._channels;
    if (!out) out = new Float32Array(frames * ch);
    const all = this._all();
    for (let c = 0; c < ch; c++) {
      const srcBase = c * frames;
      for (let i = 0; i < frames; i++) {
        out[i * ch + c] = all[srcBase + i];
      }
    }
    return out;
  }

  // --- AudioWorklet bridge ----------------------------------------------
  // Worklet hands process() planar Float32Array[ch] per bus; we use bus 0 only.
  // Channel mismatches: surplus heap channels zeroed, surplus worklet channels dropped.

  /** Copy a worklet input bus into this heap buffer. */
  copyFromAudioWorklet(inputChannels) {
    const all = this._all();
    const frames = this._frames;
    const ch = this._channels;
    const provided = inputChannels ? inputChannels.length : 0;
    for (let c = 0; c < ch; c++) {
      const dstBase = c * frames;
      if (c < provided && inputChannels[c]) {
        const src = inputChannels[c];
        const n = Math.min(frames, src.length); // src may be shorter; zero the rest
        all.set(src.subarray(0, n), dstBase);
        for (let i = n; i < frames; i++) all[dstBase + i] = 0;
      } else {
        for (let i = 0; i < frames; i++) all[dstBase + i] = 0;
      }
    }
  }

  /** Copy this heap buffer out into a worklet output bus. */
  copyToAudioWorklet(outputChannels) {
    const all = this._all();
    const frames = this._frames;
    const heapCh = this._channels;
    const outCh = outputChannels ? outputChannels.length : 0;
    for (let c = 0; c < outCh; c++) {
      const dst = outputChannels[c];
      if (!dst) continue;
      if (c < heapCh) {
        const srcBase = c * frames;
        const n = Math.min(frames, dst.length);
        dst.set(all.subarray(srcBase, srcBase + n));
        for (let i = n; i < dst.length; i++) dst[i] = 0;
      } else {
        dst.fill(0);
      }
    }
  }

  /** Release the heap allocation. The object must not be used afterwards. */
  free() {
    if (this._ptr !== 0) {
      this._m._free(this._ptr);
      this._ptr = 0;
    }
  }
}

export default HeapAudioBuffer;
