// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless DOM test for the Messages + Callbacks UI in avnd-ui.js, using a mock
// document (no jsdom) and a module stub.

import {
  buildMessagesUI,
  buildCallbacksLog,
  drainCallbacksInto,
} from "../js/avnd-ui.js";

let fail = 0;
const check = (b, m) => { console.log((b ? "ok   " : "FAIL ") + m); if (!b) fail++; };

// minimal DOM mock
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
    this.placeholder = "";
    this.scrollTop = 0;
    this.scrollHeight = 0;
  }
  appendChild(c) { this.children.push(c); return c; }
  removeChild(c) { const i = this.children.indexOf(c); if (i >= 0) this.children.splice(i, 1); return c; }
  get lastChild() { return this.children[this.children.length - 1] || null; }
  set innerHTML(v) { if (v === "") this.children = []; }
  get innerHTML() { return ""; }
  addEventListener(ev, fn) { (this._listeners[ev] ||= []).push(fn); }
  dispatch(ev) { (this._listeners[ev] || []).forEach((f) => f()); }
  set value(v) { this._value = v; }
  get value() { return this._value; }
  get type() { return this.attrs.type; }
  set type(v) { this.attrs.type = v; }
  findAll(tag, out = []) {
    for (const c of this.children) {
      if (c.tagName === tag.toUpperCase()) out.push(c);
      if (c.findAll) c.findAll(tag, out);
    }
    return out;
  }
}
globalThis.document = { createElement: (t) => new El(t) };

// module stub: "add" queues "sum"(a+b); "work" queues "bang"(42).
const messages = [
  { index: 0, name: "work", argCount: 0 },
  { index: 1, name: "add", argCount: 2 },
];
const callbacks = [
  { index: 0, name: "bang", argCount: 1 },
  { index: 1, name: "sum", argCount: 1 },
];
let queue = [];
const stub = {
  getMessageCount: () => messages.length,
  getMessageInfo: (i) => messages[i],
  getCallbackCount: () => callbacks.length,
  getCallbackInfo: (i) => callbacks[i],
  sendMessage: (name, args) => {
    if (name === "work") queue.push({ index: 0, name: "bang", args: [42] });
    else if (name === "add") queue.push({ index: 1, name: "sum", args: [(+args[0] || 0) + (+args[1] || 0)] });
    return true;
  },
  drainCallbacks: () => { const q = queue; queue = []; return q; },
};

// Mirror the standalone controller: send to instance, then drain.
let lastSent = null;
const ctx = {
  sendMessage: (name, args) => {
    lastSent = { name, args: args.slice() };
    stub.sendMessage(name, args);
    drainCallbacksInto(stub, logHandle);
  },
};

// Messages fieldset renders
const msgFs = buildMessagesUI(
  [stub.getMessageInfo(0), stub.getMessageInfo(1)],
  ctx,
);
check(!!msgFs && msgFs.tagName === "FIELDSET", "buildMessagesUI returns a <fieldset>");
const legends = msgFs.findAll("legend");
check(legends.length === 1 && legends[0].textContent === "Messages", "legend is 'Messages'");
const rows = msgFs.findAll("div").filter((d) => d.className.includes("message-row"));
check(rows.length === 2, "two message rows rendered");
const addArgInputs = rows[1].findAll("input").filter((i) => i.attrs.type === "text");
check(addArgInputs.length === 2, "'add' row has 2 arg inputs (argCount=2)");
const workArgInputs = rows[0].findAll("input").filter((i) => i.attrs.type === "text");
check(workArgInputs.length === 0, "'work' row has 0 arg inputs (argCount=0)");
const sendButtons = msgFs.findAll("button").filter((b) => b.className.includes("message-send"));
check(sendButtons.length === 2, "each row has a Send button");

// Callbacks log + round-trip
const logHandle = buildCallbacksLog(
  [stub.getCallbackInfo(0), stub.getCallbackInfo(1)],
  {},
);
check(logHandle.element.tagName === "FIELDSET", "buildCallbacksLog returns a <fieldset>");
const cbLegends = logHandle.element.findAll("legend");
check(cbLegends.length === 1 && cbLegends[0].textContent === "Callbacks", "legend is 'Callbacks'");

addArgInputs[0].value = "3";
addArgInputs[1].value = "4.5";
sendButtons[1].dispatch("click");
check(
  lastSent && lastSent.name === "add" && lastSent.args[0] === 3 && Math.abs(lastSent.args[1] - 4.5) < 1e-9,
  `Send 'add' -> sendMessage('add',[3,4.5]): ${JSON.stringify(lastSent)}`,
);

const logDiv = logHandle.element.findAll("div").find((d) => d.className === "callbacks-log");
let lines = logDiv.findAll("div").filter((d) => d.className === "callback-line");
check(lines.length === 1, "one callback line after sending 'add'");
check(lines[0].textContent === "sum(7.5)", `line is 'sum(7.5)': '${lines[0].textContent}'`);

// 'work' (0 args) queues bang(42)
sendButtons[0].dispatch("click");
lines = logDiv.findAll("div").filter((d) => d.className === "callback-line");
check(lines.length === 2 && lines[1].textContent === "bang(42)", `bang(42) appended: '${lines[1] && lines[1].textContent}'`);

// non-numeric arg passed through as a string
let strSent = null;
const ctx2 = { sendMessage: (name, args) => { strSent = args; } };
const fs2 = buildMessagesUI([{ index: 0, name: "say", argCount: 1 }], ctx2);
const sayInput = fs2.findAll("input").filter((i) => i.attrs.type === "text")[0];
sayInput.value = "hello";
fs2.findAll("button")[0].dispatch("click");
check(strSent && strSent[0] === "hello", `string arg kept as string: ${JSON.stringify(strSent)}`);

check(buildMessagesUI([], ctx) === null, "buildMessagesUI([]) returns null (no Messages section)");

console.log(fail === 0 ? "\nMESSAGES-CALLBACKS-DOM OK" : `\nMESSAGES-CALLBACKS-DOM FAIL (${fail})`);
process.exit(fail ? 1 : 0);
