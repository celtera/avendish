// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless test for the WASM Canvas2D UI backend, mocking a
// CanvasRenderingContext2D that records every call/property set.

import Module from './wasm_canvas_ui.mjs';

let failures = 0;
function check(cond, msg) {
  console.log((cond ? 'ok   ' : 'FAIL ') + msg);
  if (!cond) failures++;
}

// mock CanvasRenderingContext2D
function makeMockCtx() {
  const calls = [];
  const props = {};
  const target = {
    __calls: calls,
    __props: props,
    beginPath: (...a) => calls.push(['beginPath', ...a]),
    closePath: (...a) => calls.push(['closePath', ...a]),
    stroke: (...a) => calls.push(['stroke', ...a]),
    fill: (...a) => calls.push(['fill', ...a]),
    moveTo: (...a) => calls.push(['moveTo', ...a]),
    lineTo: (...a) => calls.push(['lineTo', ...a]),
    rect: (...a) => calls.push(['rect', ...a]),
    arc: (...a) => calls.push(['arc', ...a]),
    ellipse: (...a) => calls.push(['ellipse', ...a]),
    bezierCurveTo: (...a) => calls.push(['bezierCurveTo', ...a]),
    quadraticCurveTo: (...a) => calls.push(['quadraticCurveTo', ...a]),
    roundRect: (...a) => calls.push(['roundRect', ...a]),
    fillRect: (...a) => calls.push(['fillRect', ...a]),
    strokeRect: (...a) => calls.push(['strokeRect', ...a]),
    fillText: (...a) => calls.push(['fillText', ...a]),
    strokeText: (...a) => calls.push(['strokeText', ...a]),
    translate: (...a) => calls.push(['translate', ...a]),
    scale: (...a) => calls.push(['scale', ...a]),
    rotate: (...a) => calls.push(['rotate', ...a]),
    setTransform: (...a) => calls.push(['setTransform', ...a]),
    putImageData: (...a) => calls.push(['putImageData', '<img>', ...a.slice(1)]),
    drawImage: (...a) => calls.push(['drawImage', '<img>', ...a.slice(1)]),
    createImageData: (w, h) => {
      calls.push(['createImageData', w, h]);
      return { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) };
    },
    getContext: () => makeMockCtx(),
    // roundRect present so has_round_rect() picks it up.
  };
  return new Proxy(target, {
    set(t, prop, value) {
      props[prop] = value;
      calls.push(['set:' + String(prop), value]);
      return true;
    },
    get(t, prop) {
      return t[prop];
    },
  });
}

const M = await Module();
const p = new M.wasm_canvas_ui();
p.prepare(1, 1, 128, 48000);

check(p.hasCustomUI() === true, 'hasCustomUI() === true');
check(p.getCustomUICount() === 2, 'getCustomUICount() === 2 (meter + primitives)');

// gain = 0.25 -> bar width 0.25 * 200 = 50
p.setParameter(0, 0.25);
p.setParameter(1, 0);

const ctx = makeMockCtx();
p.paintCustom(0, ctx);
const calls = ctx.__calls;
console.log('recorded calls:');
for (const c of calls) console.log('   ', JSON.stringify(c));

check(calls[0][0] === 'beginPath', 'first call is beginPath');
const firstFill = calls.find((c) => c[0] === 'set:fillStyle');
check(!!firstFill, 'fillStyle is set before fill');

const rects = calls.filter((c) => c[0] === 'rect');
check(rects.length === 3, 'three rect() subpaths (bg, bar, outline)');
const barRect = rects[1];
check(
  barRect && Math.abs(barRect[3] - 50) < 1e-6 && Math.abs(barRect[4] - 24) < 1e-6,
  `meter bar rect width tracks gain: rect(0,0,50,24) got ${JSON.stringify(barRect)}`
);
const barColor = calls
  .filter((c) => c[0] === 'set:fillStyle')
  .map((c) => c[1]);
check(
  barColor.some((s) => s.startsWith('rgba(60,200,120')),
  `bar fill color is green: ${JSON.stringify(barColor)}`
);
check(calls.some((c) => c[0] === 'set:strokeStyle'), 'strokeStyle set for outline');
check(calls.some((c) => c[0] === 'set:lineWidth' && c[1] === 1), 'lineWidth = 1');
check(calls.some((c) => c[0] === 'stroke'), 'outline stroked');

// gain 0.8 -> bar width 160; bypass on -> red
p.setParameter(0, 0.8);
p.setParameter(1, 1);
const ctx2 = makeMockCtx();
p.paintCustom(0, ctx2);
const rects2 = ctx2.__calls.filter((c) => c[0] === 'rect');
check(
  rects2[1] && Math.abs(rects2[1][3] - 160) < 1e-3,
  `gain=0.8 -> bar width 160 (got ${rects2[1] && rects2[1][3]})`
);
const colors2 = ctx2.__calls
  .filter((c) => c[0] === 'set:fillStyle')
  .map((c) => c[1]);
check(
  colors2.some((s) => s.startsWith('rgba(200,80,80')),
  `bypass on -> red bar color: ${JSON.stringify(colors2)}`
);

// painter-primitive mapping via PrimitivesItem
const ctx3 = makeMockCtx();
p.paintCustom(1, ctx3);
const c3 = ctx3.__calls;
console.log('primitives calls:');
for (const c of c3) console.log('   ', JSON.stringify(c));
const find = (name) => c3.find((c) => c[0] === name);
check(ctx3.__props.font === '12px Arial', `font composed: ${ctx3.__props.font}`);
check(!!find('fillText') && find('fillText')[1] === 'hi', 'draw_text -> fillText("hi",x,y)');
check(!!find('translate'), 'translate forwarded');
const rot = find('rotate');
check(rot && Math.abs(rot[1] - Math.PI / 2) < 1e-9, `rotate(90deg) -> ${rot && rot[1]} rad (= pi/2)`);
check(!!find('scale'), 'scale forwarded');
const arc = find('arc');
check(arc && arc[3] === 10 && Math.abs(arc[5] - 2 * Math.PI) < 1e-9, `draw_circle -> arc(_,_,10,0,2pi): ${JSON.stringify(arc)}`);
const ell = c3.filter((c) => c[0] === 'ellipse');
check(ell[0] && ell[0][1] === 10 && ell[0][2] === 5 && ell[0][3] === 10 && ell[0][4] === 5,
  `draw_ellipse bbox->centre/radii: ${JSON.stringify(ell[0])}`);
const arcTo = ell[1];
check(arcTo && arcTo[1] === 10 && arcTo[2] === 10 && arcTo[3] === 10 && arcTo[4] === 10,
  `arc_to centre/radii: ${JSON.stringify(arcTo)}`);
check(arcTo && Math.abs(arcTo[6] - 0) < 1e-9 && Math.abs(arcTo[7] + Math.PI / 2) < 1e-9 && arcTo[8] === true,
  `arc_to angles: start 0, end -pi/2, anticlockwise=true: ${JSON.stringify(arcTo)}`);
check(!!find('moveTo') && !!find('lineTo'), 'draw_line -> moveTo + lineTo');
const st = find('setTransform');
check(st && st[1] === 1 && st[2] === 0 && st[3] === 0 && st[4] === 1 && st[5] === 0 && st[6] === 0,
  `reset_transform -> setTransform identity: ${JSON.stringify(st)}`);
check(!!find('putImageData'), 'draw_bytes (unscaled) -> putImageData');

// getUILayout structure
const layout = p.getUILayout();
console.log('layout:', JSON.stringify(layout, null, 2));
check(layout && layout.type === 'vbox', 'root is vbox');
const kids = layout.children || [];
check(kids.length === 3, 'root has 3 children (group + 2 custom)');
const group = kids[0];
check(group && group.type === 'group', 'first child is a group');
check(group && group.name === 'Controls', 'group name is "Controls"');
const gkids = (group && group.children) || [];
check(gkids.length === 2, 'group has 2 control children');
check(gkids[0] && gkids[0].type === 'control' && gkids[0].index === 0, 'control[0] -> param 0 (Gain)');
check(gkids[1] && gkids[1].type === 'control' && gkids[1].index === 1, 'control[1] -> param 1 (Bypass)');
const custom = kids[1];
check(custom && custom.type === 'custom', 'second child is custom');
check(custom && custom.itemIndex === 0, 'custom itemIndex === 0');
check(custom && custom.width === 200 && custom.height === 24, 'custom is 200x24');
const custom2 = kids[2];
check(custom2 && custom2.type === 'custom' && custom2.itemIndex === 1, 'third child is custom itemIndex 1');
check(custom2 && custom2.width === 64 && custom2.height === 64, 'primitives item is 64x64');

p.delete();

console.log(failures === 0 ? '\nCANVAS-UI OK' : `\nCANVAS-UI FAIL (${failures} failures)`);
process.exit(failures === 0 ? 0 : 1);
