/* SPDX-License-Identifier: GPL-3.0-or-later */
// Test doubles for @webaudiomodules/sdk + browser globals, just enough surface
// to exercise node.js param/state logic in plain Node. Not a real WAM runtime.

class FakePort extends EventTarget {
  constructor() {
    super();
    this.sent = [];
    this._peer = null;
  }
  postMessage(data) {
    this.sent.push(data);
    if (this._peer) {
      // deliver async like a real MessagePort
      queueMicrotask(() =>
        this._peer.dispatchEvent(new MessageEventLike('message', data)),
      );
    }
  }
  start() {}
}

class MessageEventLike extends Event {
  constructor(type, data) {
    super(type);
    this.data = data;
  }
}

if (typeof globalThis.CustomEvent === 'undefined') {
  globalThis.CustomEvent = class CustomEvent extends Event {
    constructor(type, init) {
      super(type);
      this.detail = init && init.detail;
    }
  };
}

// Fake AudioWorkletNode base.
class FakeAudioWorkletNode extends EventTarget {
  constructor(context, name, options) {
    super();
    this.context = context;
    this.processorName = name;
    this.options = options || {};
    this.port = new FakePort();
  }
}

// SDK WamNode stand-in.
export class WamNode extends FakeAudioWorkletNode {
  constructor(module, options) {
    super(module && module.audioContext, '@wam-processor', options);
    this.module = module;
    this._wamScheduled = [];
  }
  async _initialize() {
    /* SDK ParamMgr handshake — no-op in tests */
  }
  scheduleEvents(...events) {
    this._wamScheduled.push(...events);
  }
  clearEvents() {
    this._wamScheduled.length = 0;
  }
}

export class WebAudioModule extends EventTarget {
  static get isWebAudioModuleConstructor() {
    return true;
  }
  constructor(groupId, audioContext) {
    super();
    this.groupId = groupId;
    this.audioContext = audioContext;
  }
  async initialize(state) {
    this.state = state;
    return this;
  }
  static async createInstance(groupId, audioContext, initialState) {
    const inst = new this(groupId, audioContext);
    await inst.initialize(initialState);
    return inst;
  }
}

export async function addFunctionModule(/* worklet, fn, moduleId */) {
  return undefined;
}

globalThis.AvndWamBase = globalThis.AvndWamBase || {};
globalThis.AvndWamBase.WamNode = WamNode;

export default { WamNode, WebAudioModule, addFunctionModule };
