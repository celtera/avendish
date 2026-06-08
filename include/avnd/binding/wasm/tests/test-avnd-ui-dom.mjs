// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless DOM test for avnd-ui.js, using a mock document (no jsdom).

import { buildLayoutUI, buildAutoUI, makeControl } from "../js/avnd-ui.js";

let fail = 0;
const check = (b, m) => { console.log((b ? "ok   " : "FAIL ") + m); if (!b) fail++; };

// minimal DOM mock
let rafQueue = [];
class El {
  constructor(tag) {
    this.tagName = tag.toUpperCase();
    this.children = [];
    this.style = {};
    this.attrs = {};
    this._listeners = {};
    this.textContent = "";
    this.className = "";
    this._value = "";
  }
  appendChild(c) { this.children.push(c); return c; }
  set innerHTML(v) { if (v === "") this.children = []; }
  get innerHTML() { return ""; }
  addEventListener(ev, fn) { (this._listeners[ev] ||= []).push(fn); }
  dispatch(ev) { (this._listeners[ev] || []).forEach((f) => f()); }
  set value(v) { this._value = v; }
  get value() { return this._value; }
  get type() { return this.attrs.type; }
  set type(v) { this.attrs.type = v; }
  getContext() {
    const calls = [];
    return new Proxy({ __calls: calls, setTransform: () => calls.push("setTransform"), clearRect: () => calls.push("clearRect") }, {
      get(t, p) { return t[p] || (() => calls.push(String(p))); },
      set() { return true; },
    });
  }
  findAll(tag, out = []) {
    for (const c of this.children) {
      if (c.tagName === tag.toUpperCase()) out.push(c);
      if (c.findAll) c.findAll(tag, out);
    }
    return out;
  }
}
globalThis.document = { createElement: (t) => new El(t) };
globalThis.window = { devicePixelRatio: 1 };
globalThis.requestAnimationFrame = (fn) => { rafQueue.push(fn); return rafQueue.length; };
globalThis.cancelAnimationFrame = () => {};

// fixtures
const paramInfos = [
  { index: 0, name: "Gain", type: "float", widget: "slider", min: 0, max: 1, init: 0.5 },
  { index: 1, name: "Bypass", type: "bool", widget: "toggle", init: 0 },
];
const layoutTree = {
  type: "vbox",
  name: "Main",
  children: [
    { type: "group", name: "Controls", children: [
      { type: "control", index: 0 },
      { type: "control", index: 1 },
    ] },
    { type: "custom", itemIndex: 0, width: 200, height: 24, fullyCustom: false },
  ],
};

let lastSet = null;
let paintCalls = [];
let mirrorCalls = 0;
const uiInstance = {
  paintCustom: (idx, ctx) => paintCalls.push(idx),
  setParameter: () => {},
};
const ctx = {
  paramInfos,
  setParam: (i, v) => { lastSet = [i, v]; },
  getParam: (i) => paramInfos[i].init,
  uiInstance,
  mirror: () => { mirrorCalls++; },
};

// structured render
const root = new El("div");
const handle = buildLayoutUI(root, layoutTree, ctx);
const groups = root.findAll("fieldset");
check(groups.length === 1, "one <fieldset> group rendered");
const legends = root.findAll("legend");
check(legends.length === 1 && legends[0].textContent === "Controls", "group legend is 'Controls'");
const rows = root.findAll("div").filter((d) => d.className === "param-row");
check(rows.length === 2, "two control rows rendered");
const canvases = root.findAll("canvas");
check(canvases.length === 1, "one <canvas> for custom item");
check(canvases[0].width === 200 && canvases[0].height === 24, "canvas sized 200x24");

// animation loop calls paintCustom + mirror
check(rafQueue.length >= 1, "rAF scheduled for custom canvas");
const frame = rafQueue.shift();
frame();
check(paintCalls.length === 1 && paintCalls[0] === 0, "paintCustom(0, ctx2d) called once");
check(mirrorCalls === 1, "mirror() called before paint");
handle.stop();

// editing a control calls setParam
const sliderInputs = root.findAll("input").filter((i) => i.attrs.type === "range");
check(sliderInputs.length === 1, "gain rendered as a range input");
sliderInputs[0].value = "0.75";
sliderInputs[0].dispatch("input");
check(lastSet && lastSet[0] === 0 && Math.abs(lastSet[1] - 0.75) < 1e-9, `edit -> setParam(0, 0.75): ${JSON.stringify(lastSet)}`);
const checkboxes = root.findAll("input").filter((i) => i.attrs.type === "checkbox");
check(checkboxes.length === 1, "bypass rendered as a checkbox");

// flat fallback when uiInstance has no getUILayout
rafQueue = [];
const root2 = new El("div");
const flatInstance = { setParameter: () => {} };
buildAutoUI(root2, flatInstance, { paramInfos, setParam: () => {}, getParam: (i) => paramInfos[i].init });
const flatRows = root2.findAll("div").filter((d) => d.className === "param-row");
check(flatRows.length === 2, "flat fallback renders one row per parameter");

const selInfo = { index: 9, type: "enum", widget: "combobox", values: ["a", "b"], init: 1 };
const sel = makeControl(selInfo, { onChange: () => {} });
check(sel.findAll("select").length === 1, "enum -> <select>");

console.log(fail === 0 ? "\nAVND-UI-DOM OK" : `\nAVND-UI-DOM FAIL (${fail})`);
process.exit(fail ? 1 : 0);
