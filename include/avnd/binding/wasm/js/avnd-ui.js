/* SPDX-License-Identifier: GPL-3.0-or-later */
// Reusable auto-UI for Avendish WASM plug-ins: widget mapping + structured
// layout renderer. Host-agnostic via a `ctx` for reading/writing parameters.

// Build the editable control element for one parameter descriptor `info`.
export function makeControl(info, opts = {}) {
  const onChange = opts.onChange || (() => {});
  const wrap = document.createElement("div");
  wrap.className = "control";
  const type = info.type;
  const widget = info.widget;
  const init = (opts.getValue && opts.getValue()) ?? info.init;

  // enum / combobox with explicit choices -> <select>
  if ((type === "enum" || (Array.isArray(info.values) && info.values.length)) && info.values) {
    const sel = document.createElement("select");
    info.values.forEach((name, i) => {
      const o = document.createElement("option");
      o.value = String(i);
      o.textContent = name;
      sel.appendChild(o);
    });
    if (typeof init === "number") sel.value = String(init);
    else if (typeof init === "string") {
      const idx = info.values.indexOf(init);
      if (idx >= 0) sel.value = String(idx);
    }
    sel.addEventListener("change", () => onChange(+sel.value));
    wrap.appendChild(sel);
    return wrap;
  }

  // bool / toggle -> checkbox
  if (type === "bool" || widget === "toggle") {
    const cb = document.createElement("input");
    cb.type = "checkbox";
    cb.checked = !!init;
    cb.addEventListener("change", () => onChange(cb.checked ? 1 : 0));
    wrap.appendChild(cb);
    return wrap;
  }

  // color -> <input type=color>; sends a normalized [r,g,b,a] vector
  if (type === "rgb" || type === "rgba" || widget === "color") {
    const col = document.createElement("input");
    col.type = "color";
    col.value = "#808080";
    const sendColor = () => {
      const hex = col.value;
      const r = parseInt(hex.slice(1, 3), 16) / 255;
      const g = parseInt(hex.slice(3, 5), 16) / 255;
      const b = parseInt(hex.slice(5, 7), 16) / 255;
      onChange(type === "rgba" ? [r, g, b, 1] : [r, g, b]);
    };
    col.addEventListener("input", sendColor);
    wrap.appendChild(col);
    return wrap;
  }

  // string / lineedit -> text input
  if (type === "string" || widget === "lineedit") {
    const t = document.createElement("input");
    t.type = "text";
    t.value = init != null ? String(init) : "";
    t.addEventListener("change", () => onChange(t.value));
    wrap.appendChild(t);
    return wrap;
  }

  // spinbox / iknob -> number input
  if (widget === "spinbox" || widget === "iknob") {
    const n = document.createElement("input");
    n.type = "number";
    if (info.min != null) n.min = info.min;
    if (info.max != null) n.max = info.max;
    n.step = type === "int" ? 1 : (info.step ?? "any");
    n.value = init ?? info.min ?? 0;
    n.addEventListener("input", () => onChange(+n.value));
    wrap.appendChild(n);
    return wrap;
  }

  // default: numeric slider (slider / knob / float / int)
  const min = info.min != null ? info.min : 0;
  const max = info.max != null ? info.max : 1;
  const isInt = type === "int";
  const step = info.step != null ? info.step : isInt ? 1 : (max - min) / 1000 || 0.001;

  const range = document.createElement("input");
  range.type = "range";
  range.min = min;
  range.max = max;
  range.step = step;
  range.value = init != null ? init : min;

  const readout = document.createElement("span");
  readout.className = "readout";
  readout.textContent = (+range.value).toFixed(isInt ? 0 : 3);

  range.addEventListener("input", () => {
    const v = isInt ? Math.round(+range.value) : +range.value;
    readout.textContent = isInt ? String(v) : v.toFixed(3);
    onChange(v);
  });

  wrap.appendChild(range);
  wrap.appendChild(readout);
  return wrap;
}

// Render one labelled control row.
function controlRow(info, ctx) {
  const row = document.createElement("div");
  row.className = "param-row";
  const label = document.createElement("label");
  label.textContent = info.name || info.identifier || `param ${info.index}`;
  row.appendChild(label);
  const control = makeControl(info, {
    onChange: (v) => ctx.setParam(info.index, v),
    getValue: ctx.getParam ? () => ctx.getParam(info.index) : undefined,
  });
  row.appendChild(control);
  return row;
}

// A titled section (<fieldset><legend>) holding a set of rows.
function section(title) {
  const fs = document.createElement("fieldset");
  fs.className = "avnd-section";
  const lg = document.createElement("legend");
  lg.textContent = title;
  fs.appendChild(lg);
  return fs;
}

// Flat fallback UI: "control" params -> Controls section, "value" -> Inputs.
export function buildFlatParamUI(root, infos, ctx) {
  root.innerHTML = "";
  const controls = [];
  const values = [];
  for (const info of infos) {
    if (!info) continue;
    (info.kind === "value" ? values : controls).push(info);
  }

  if (controls.length) {
    const fs = section("Controls");
    for (const info of controls) fs.appendChild(controlRow(info, ctx));
    root.appendChild(fs);
  }
  if (values.length) {
    const fs = section("Inputs");
    for (const info of values) fs.appendChild(controlRow(info, ctx));
    root.appendChild(fs);
  }
  if (!controls.length && !values.length) {
    const p = document.createElement("p");
    p.className = "avnd-empty";
    p.textContent = "(no parameters)";
    root.appendChild(p);
  }
}

// --- Messages (JS -> plug-in named RPC) ---

// Parse as number when it looks numeric, else keep the string; the C++ dispatcher
// would reject a numeric string for a numeric arg.
function parseArg(raw) {
  if (raw === "" || raw == null) return "";
  const t = raw.trim();
  if (t !== "" && !Number.isNaN(Number(t))) return Number(t);
  return raw;
}

// "Messages" fieldset: one row per descriptor, with arg inputs and a Send button.
// Returns the fieldset, or null when there are no messages.
export function buildMessagesUI(msgInfos, ctx) {
  if (!msgInfos || !msgInfos.length) return null;
  const fs = section("Messages");
  for (const info of msgInfos) {
    if (!info) continue;
    const row = document.createElement("div");
    row.className = "param-row message-row";
    const label = document.createElement("label");
    label.textContent = info.name || `message ${info.index}`;
    row.appendChild(label);

    const argInputs = [];
    const argCount = info.argCount || 0;
    for (let a = 0; a < argCount; a++) {
      const inp = document.createElement("input");
      inp.type = "text";
      inp.className = "message-arg";
      inp.placeholder = `arg ${a}`;
      row.appendChild(inp);
      argInputs.push(inp);
    }

    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "message-send";
    btn.textContent = "Send";
    btn.addEventListener("click", () => {
      const args = argInputs.map((i) => parseArg(i.value));
      if (ctx.sendMessage) ctx.sendMessage(info.name, args);
    });
    row.appendChild(btn);
    fs.appendChild(row);
  }
  return fs;
}

// --- Callbacks (plug-in -> JS) log ---

// "Callbacks" fieldset with a capped scrolling log. Returns { element, append, clear }.
export function buildCallbacksLog(cbInfos, opts = {}) {
  const max = opts.max || 200;
  const fs = section("Callbacks");
  const log = document.createElement("div");
  log.className = "callbacks-log";
  log.style.fontFamily = "monospace";
  log.style.fontSize = "0.85em";
  log.style.maxHeight = (opts.height || 160) + "px";
  log.style.overflowY = "auto";
  log.style.whiteSpace = "pre-wrap";
  fs.appendChild(log);

  if (cbInfos && cbInfos.length) {
    const hdr = document.createElement("div");
    hdr.className = "callbacks-header";
    hdr.style.opacity = "0.6";
    hdr.textContent =
      "// " + cbInfos.filter(Boolean).map((c) => `${c.name}/${c.argCount}`).join(", ");
    log.appendChild(hdr);
  }

  function append(event) {
    if (!event) return;
    const line = document.createElement("div");
    line.className = "callback-line";
    const args = Array.isArray(event.args) ? event.args : [];
    const argStr = args
      .map((v) => (typeof v === "number" ? formatNum(v) : JSON.stringify(v)))
      .join(", ");
    line.textContent = `${event.name || "cb" + event.index}(${argStr})`;
    log.appendChild(line);
    while (log.children.length > max + (cbInfos && cbInfos.length ? 1 : 0)) {
      const removable = (cbInfos && cbInfos.length) ? log.children[1] : log.children[0]; // keep header
      if (!removable) break;
      log.removeChild(removable);
    }
    log.scrollTop = log.scrollHeight;
  }

  function clear() {
    while (log.lastChild) log.removeChild(log.lastChild);
  }

  return { element: fs, append, clear };
}

function formatNum(v) {
  if (Number.isInteger(v)) return String(v);
  return (+v).toFixed(4).replace(/\.?0+$/, "");
}

// Drain instance.drainCallbacks() into a buildCallbacksLog handle.
export function drainCallbacksInto(instance, logHandle) {
  if (!instance || !logHandle || typeof instance.drainCallbacks !== "function")
    return 0;
  let events;
  try {
    events = instance.drainCallbacks();
  } catch {
    return 0;
  }
  if (!events) return 0;
  const n = events.length || 0;
  for (let i = 0; i < n; i++) {
    const e = events[i];
    if (!e) continue;
    const raw = e.args; // emscripten::val array -> JS array
    const args = [];
    if (raw) {
      const m = raw.length || 0;
      for (let j = 0; j < m; j++) args.push(raw[j]);
    }
    logHandle.append({ index: e.index, name: e.name, args });
  }
  return n;
}

// --- Structured layout renderer ---

// Render `layoutTree` (from module.getUILayout()) into `root`.
// ctx = { paramInfos, setParam, getParam?, uiInstance?, mirror? }.
// Returns { stop() } to cancel custom-canvas animation loops.
export function buildLayoutUI(root, layoutTree, ctx) {
  root.innerHTML = "";
  const loops = [];
  renderNode(root, layoutTree, ctx, loops);
  return {
    stop() {
      for (const l of loops) l.stop();
      loops.length = 0;
    },
  };
}

function renderNode(parent, node, ctx, loops) {
  if (!node) return;
  switch (node.type) {
    case "control": {
      const info = ctx.paramInfos[node.index];
      if (info) parent.appendChild(controlRow(info, ctx));
      else {
        // descriptor missing: generic numeric control
        parent.appendChild(controlRow({ index: node.index, type: "float", widget: "slider" }, ctx));
      }
      return;
    }
    case "label": {
      const l = document.createElement("div");
      l.className = "ui-label";
      l.textContent = node.text || "";
      parent.appendChild(l);
      return;
    }
    case "spacing": {
      const s = document.createElement("div");
      s.style.width = (node.width || 0) + "px";
      s.style.height = (node.height || 0) + "px";
      parent.appendChild(s);
      return;
    }
    case "custom": {
      parent.appendChild(makeCustomCanvas(node, ctx, loops));
      return;
    }
    case "tabs":
      parent.appendChild(makeTabs(node, ctx, loops));
      return;
    case "group": {
      const fs = document.createElement("fieldset");
      fs.className = "ui-group";
      if (node.name) {
        const lg = document.createElement("legend");
        lg.textContent = node.name;
        fs.appendChild(lg);
      }
      renderChildren(fs, node, ctx, loops);
      parent.appendChild(fs);
      return;
    }
    case "hbox":
    case "split":
    case "grid": {
      const d = document.createElement("div");
      d.className = "ui-" + node.type;
      d.style.display = node.type === "grid" ? "grid" : "flex";
      if (node.type === "grid")
        d.style.gridTemplateColumns = `repeat(${node.columns || 2}, 1fr)`;
      else d.style.flexDirection = "row";
      d.style.gap = "0.6rem";
      renderChildren(d, node, ctx, loops);
      parent.appendChild(d);
      return;
    }
    case "vbox":
    case "container":
    default: {
      const d = document.createElement("div");
      d.className = "ui-" + (node.type || "container");
      d.style.display = "flex";
      d.style.flexDirection = "column";
      renderChildren(d, node, ctx, loops);
      parent.appendChild(d);
      return;
    }
  }
}

function renderChildren(parent, node, ctx, loops) {
  for (const c of node.children || []) renderNode(parent, c, ctx, loops);
}

function makeTabs(node, ctx, loops) {
  const wrap = document.createElement("div");
  wrap.className = "ui-tabs";
  const bar = document.createElement("div");
  bar.className = "ui-tabbar";
  bar.style.display = "flex";
  bar.style.gap = "0.3rem";
  const body = document.createElement("div");
  body.className = "ui-tabbody";

  const children = node.children || [];
  const panels = [];
  children.forEach((child, i) => {
    const btn = document.createElement("button");
    btn.textContent = child.name || `Tab ${i + 1}`;
    const panel = document.createElement("div");
    panel.style.display = i === 0 ? "block" : "none";
    renderNode(panel, child, ctx, loops);
    btn.addEventListener("click", () => {
      panels.forEach((p, k) => (p.style.display = k === i ? "block" : "none"));
    });
    bar.appendChild(btn);
    panels.push(panel);
    body.appendChild(panel);
  });
  wrap.appendChild(bar);
  wrap.appendChild(body);
  return wrap;
}

// Custom paint item -> <canvas> + rAF loop calling paintCustom.
function makeCustomCanvas(node, ctx, loops) {
  const canvas = document.createElement("canvas");
  const dpr = (typeof window !== "undefined" && window.devicePixelRatio) || 1;
  const w = node.width || 100;
  const h = node.height || 100;
  canvas.width = Math.round(w * dpr);
  canvas.height = Math.round(h * dpr);
  canvas.style.width = w + "px";
  canvas.style.height = h + "px";
  canvas.className = "ui-custom";
  if (!node.fullyCustom) canvas.style.border = "1px solid #8884";

  const c2d = canvas.getContext("2d");
  let raf = 0;
  let running = true;

  const frame = () => {
    if (!running) return;
    if (ctx.uiInstance && typeof ctx.uiInstance.paintCustom === "function") {
      // Mirror live params into the paint-only instance before painting.
      if (ctx.mirror) ctx.mirror();
      c2d.setTransform(dpr, 0, 0, dpr, 0, 0);
      c2d.clearRect(0, 0, w, h);
      try {
        ctx.uiInstance.paintCustom(node.itemIndex, c2d);
      } catch (e) {
        running = false;
        console.error("paintCustom failed:", e);
        return;
      }
    }
    raf = requestAnimationFrame(frame);
  };
  raf = requestAnimationFrame(frame);

  loops.push({
    stop() {
      running = false;
      if (raf) cancelAnimationFrame(raf);
    },
  });
  return canvas;
}

// Build the best UI we can: structured layout if available, else flat params.
// ctx = { paramInfos, setParam, getParam?, mirror? }.
export function buildAutoUI(root, uiInstance, ctx) {
  let tree;
  try {
    tree = uiInstance.getUILayout ? uiInstance.getUILayout() : null;
  } catch {
    tree = null;
  }
  ctx.uiInstance = uiInstance;
  if (tree && tree.type) {
    return buildLayoutUI(root, tree, ctx);
  }
  buildFlatParamUI(root, ctx.paramInfos, ctx);
  return { stop() {} };
}
