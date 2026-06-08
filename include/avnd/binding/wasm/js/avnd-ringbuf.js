/* SPDX-License-Identifier: GPL-3.0-or-later */

// Wait-free SPSC ring buffer. Byte layout matches the C++ wasm::ring_buffer
// (ringbuffer.hpp) and padenot/ringbuf.js: [read u32][write u32][capacity bytes].
// capacity must be a power of two (indices masked by capacity-1).

const HEADER_BYTES = 8;
const READ_INDEX = 0;
const WRITE_INDEX = 1;

function isPowerOfTwo(n) {
  return n > 0 && (n & (n - 1)) === 0;
}

export class RingBuffer {
  /** Backing byte length for `capacityBytes` of storage (power of two). */
  static storageSize(capacityBytes) {
    if (!isPowerOfTwo(capacityBytes)) {
      throw new RangeError(
        `RingBuffer capacity must be a power of two, got ${capacityBytes}`,
      );
    }
    return capacityBytes + HEADER_BYTES;
  }

  constructor(sab) {
    if (sab.byteLength < HEADER_BYTES + 1) {
      throw new RangeError("RingBuffer backing buffer too small");
    }
    const capacity = sab.byteLength - HEADER_BYTES;
    if (!isPowerOfTwo(capacity)) {
      throw new RangeError(
        `RingBuffer storage (total - 8 = ${capacity}) must be a power of two`,
      );
    }
    this._sab = sab;
    this._capacity = capacity;
    this._mask = capacity - 1;
    // Atomics need an Int32Array; indices are logically uint32, kept unsigned via >>> 0.
    this._idx = new Int32Array(sab, 0, 2);
    this._data = new Uint8Array(sab, HEADER_BYTES, capacity);
  }

  get capacity() {
    return this._capacity;
  }

  get buffer() {
    return this._sab;
  }

  /** Bytes currently available to read (writeIdx - readIdx). */
  availableRead() {
    const rd = Atomics.load(this._idx, READ_INDEX) >>> 0;
    const wr = Atomics.load(this._idx, WRITE_INDEX) >>> 0;
    return (wr - rd) >>> 0;
  }

  /** Bytes currently available to write (capacity - availableRead). */
  availableWrite() {
    return (this._capacity - this.availableRead()) >>> 0;
  }

  /**
   * Producer: push up to `src.length` bytes. Returns the count actually written
   * (may be less if the buffer is near-full; 0 if completely full).
   * @param {Uint8Array} src
   * @returns {number}
   */
  push(src) {
    const len = src.length;
    const rd = Atomics.load(this._idx, READ_INDEX) >>> 0;
    const wr = Atomics.load(this._idx, WRITE_INDEX) >>> 0;

    const free = (this._capacity - ((wr - rd) >>> 0)) >>> 0;
    const toWrite = len < free ? len : free;
    if (toWrite === 0) return 0;

    const mask = this._mask;
    const offset = wr & mask;
    const first = this._capacity - offset < toWrite ? this._capacity - offset : toWrite;

    this._data.set(src.subarray(0, first), offset);
    if (toWrite > first) {
      this._data.set(src.subarray(first, toWrite), 0);
    }

    Atomics.store(this._idx, WRITE_INDEX, (wr + toWrite) | 0); // release
    return toWrite;
  }

  /** Consumer: pop up to `dst.length` bytes into `dst`. Returns count read. */
  pop(dst) {
    const len = dst.length;
    const rd = Atomics.load(this._idx, READ_INDEX) >>> 0;
    const wr = Atomics.load(this._idx, WRITE_INDEX) >>> 0; // acquire

    const filled = (wr - rd) >>> 0;
    const toRead = len < filled ? len : filled;
    if (toRead === 0) return 0;

    const mask = this._mask;
    const offset = rd & mask;
    const first = this._capacity - offset < toRead ? this._capacity - offset : toRead;

    dst.set(this._data.subarray(offset, offset + first), 0);
    if (toRead > first) {
      dst.set(this._data.subarray(0, toRead - first), first);
    }

    Atomics.store(this._idx, READ_INDEX, (rd + toRead) | 0); // release
    return toRead;
  }

  /** Consumer-side reset: discard everything currently readable. */
  clear() {
    const wr = Atomics.load(this._idx, WRITE_INDEX) | 0;
    Atomics.store(this._idx, READ_INDEX, wr);
  }

  // --- Float32 convenience -------------------------------------------------

  /** Push a Float32Array, all-or-nothing (partial push would corrupt framing). */
  pushFloats(floats) {
    const bytes = new Uint8Array(floats.buffer, floats.byteOffset, floats.byteLength);
    if (this.availableWrite() < bytes.length) return 0;
    const n = this.push(bytes);
    return n >>> 2;
  }

  /** Pop up to `dst.length` whole floats. Returns floats read. */
  popFloats(dst) {
    const wantBytes = dst.length * 4;
    const avail = this.availableRead();
    const readableFloats = Math.min(dst.length, (avail >>> 2));
    if (readableFloats === 0) return 0;
    const view = new Uint8Array(dst.buffer, dst.byteOffset, readableFloats * 4);
    const n = this.pop(view);
    return n >>> 2;
  }
}

export default RingBuffer;
