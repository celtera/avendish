/* SPDX-License-Identifier: GPL-3.0-or-later */
// Tests the Avendish -> WAM2 param mapping + node.js WamNode logic, with the SDK
// stubbed by ./sdk-shim.mjs. Also maps real built modules if present.
// Usage: node test-wam-params.mjs [modules-dir]  (default /tmp/avnd-wasm-test)

import assert from 'node:assert/strict';
import { readFileSync, existsSync, writeFileSync, mkdtempSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, dirname } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { buildWamParameterInfo, avndParamToWam } from '../avnd-wam-params.js';
import './sdk-shim.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const WAM_DIR = join(HERE, '..');
const MODULES_DIR = process.argv[2] || '/tmp/avnd-wasm-test';

let failures = 0;
const _pending = [];
function check(name, fn) {
  const p = (async () => {
    try {
      await fn();
      console.log(`  ok   ${name}`);
    } catch (e) {
      failures++;
      console.error(`  FAIL ${name}\n       ${e.message}`);
    }
  })();
  _pending.push(p);
  return p;
}

// --- Pure mapping unit tests (no wasm needed) -------------------------------

console.log('== avndParamToWam (synthetic descriptors) ==');

check('float maps to WAM float with range + step=0', () => {
  const [w] = avndParamToWam({
    index: 0,
    identifier: 'weight',
    name: 'Weight',
    widget: 'hslider',
    type: 'float',
    min: 0,
    max: 1,
    init: 0.5,
  });
  assert.equal(w.type, 'float');
  assert.equal(w.id, 'weight');
  assert.equal(w.label, 'Weight');
  assert.equal(w.minValue, 0);
  assert.equal(w.maxValue, 1);
  assert.equal(w.defaultValue, 0.5);
  assert.equal(w.discreteStep, 0);
});

check('int maps to WAM int with discreteStep 1', () => {
  const [w] = avndParamToWam({
    index: 1,
    identifier: 'count',
    name: 'Count',
    widget: 'knob',
    type: 'int',
    min: 0,
    max: 16,
    init: 4,
  });
  assert.equal(w.type, 'int');
  assert.equal(w.defaultValue, 4);
  assert.equal(w.discreteStep, 1);
  assert.equal(w.maxValue, 16);
});

check('bool maps to WAM boolean 0..1', () => {
  const [w] = avndParamToWam({
    index: 2,
    identifier: 'bypass',
    name: 'Bypass',
    widget: 'toggle',
    type: 'bool',
    init: 0,
  });
  assert.equal(w.type, 'boolean');
  assert.equal(w.minValue, 0);
  assert.equal(w.maxValue, 1);
  assert.equal(w.discreteStep, 1);
});

check('enum maps to WAM choice with choices[]', () => {
  const [w] = avndParamToWam({
    index: 3,
    identifier: 'mode',
    name: 'Mode',
    widget: 'enumeration',
    type: 'enum',
    min: 0,
    max: 2,
    values: ['Low', 'Mid', 'High'],
  });
  assert.equal(w.type, 'choice');
  assert.deepEqual(w.choices, ['Low', 'Mid', 'High']);
  assert.equal(w.minValue, 0);
  assert.equal(w.maxValue, 2);
  assert.equal(w.discreteStep, 1);
});

check('rgba expands to 4 normalized float components', () => {
  const ws = avndParamToWam({
    index: 4,
    identifier: 'color',
    name: 'Color',
    widget: 'color',
    type: 'rgba',
    components: 4,
  });
  assert.equal(ws.length, 4);
  assert.deepEqual(
    ws.map((w) => w.id),
    ['color/r', 'color/g', 'color/b', 'color/a'],
  );
  for (const w of ws) {
    assert.equal(w.type, 'float');
    assert.equal(w.minValue, 0);
    assert.equal(w.maxValue, 1);
  }
});

check('xy expands to 2 float components with declared range', () => {
  const ws = avndParamToWam({
    index: 5,
    identifier: 'xy',
    name: 'XY',
    widget: 'xy',
    type: 'xy',
    components: 2,
    min: -1,
    max: 1,
    init: 0,
  });
  assert.equal(ws.length, 2);
  assert.deepEqual(
    ws.map((w) => w.id),
    ['xy/x', 'xy/y'],
  );
  assert.equal(ws[0].minValue, -1);
  assert.equal(ws[0].maxValue, 1);
});

check('string/list/struct are skipped (no WAM param)', () => {
  assert.equal(avndParamToWam({ index: 6, type: 'string' }).length, 0);
  assert.equal(avndParamToWam({ index: 7, type: 'list' }).length, 0);
  assert.equal(avndParamToWam({ index: 8, type: 'bang' }).length, 0);
});

// --- node.js.in WamNode logic (placeholders substituted) --------------------

async function loadNodeModule() {
  const src = readFileSync(join(WAM_DIR, 'node.js.in'), 'utf8')
    .replace(/@AVND_WASM_C_NAME@/g, 'avnd_test')
    // point params import at the real absolute file
    .replace(
      "import { buildWamParameterInfo } from './avnd-wam-params.js';",
      `import { buildWamParameterInfo } from ${JSON.stringify(
        pathToFileURL(join(WAM_DIR, 'avnd-wam-params.js')).href,
      )};`,
    )
    // rewrite the SDK WamNode import to the stub
    .replace(
      "import { WamNode as SdkWamNode } from './sdk-wamnode.js';",
      `import { WamNode as SdkWamNode } from ${JSON.stringify(
        pathToFileURL(join(HERE, 'sdk-shim.mjs')).href,
      )};`,
    );
  const tmp = mkdtempSync(join(tmpdir(), 'avnd-wam-'));
  const f = join(tmp, 'node.mjs');
  writeFileSync(f, src);
  return import(pathToFileURL(f).href);
}

async function testNode() {
  console.log('== node.js WamNode logic ==');
  const { default: AvndWamNode } = await loadNodeModule();

  const avndInfos = [
    { index: 0, identifier: 'weight', name: 'Weight', widget: 'hslider', type: 'float', min: 0, max: 1, init: 0.5 },
    { index: 1, identifier: 'bypass', name: 'Bypass', widget: 'toggle', type: 'bool', init: 0 },
    { index: 2, identifier: 'mode', name: 'Mode', widget: 'enumeration', type: 'enum', min: 0, max: 2, values: ['Low', 'Mid', 'High'] },
  ];

  const fakeModule = { audioContext: { sampleRate: 48000 } };
  const node = new AvndWamNode(fakeModule, {
    processorOptions: { avndParamInfos: avndInfos, sampleRate: 48000 },
  });

  await check('getParameterInfo returns all ids', async () => {
    const info = await node.getParameterInfo();
    assert.deepEqual(Object.keys(info).sort(), ['bypass', 'mode', 'weight']);
    assert.equal(info.weight.type, 'float');
    assert.equal(info.bypass.type, 'boolean');
    assert.equal(info.mode.type, 'choice');
  });

  await check('default parameter values come from descriptors', async () => {
    const v = await node.getParameterValues(false);
    assert.equal(v.weight.value, 0.5);
    assert.equal(v.bypass.value, 0);
    assert.equal(v.mode.value, 0);
  });

  await check('setParameterValues posts avnd/setParam to processor', async () => {
    node.port.sent.length = 0;
    await node.setParameterValues({ weight: { id: 'weight', value: 0.25 } });
    const msg = node.port.sent.find((m) => m.type === 'avnd/setParam');
    assert.ok(msg, 'expected avnd/setParam message');
    assert.equal(msg.avndIndex, 0);
    assert.equal(msg.value, 0.25);
  });

  await check('normalized round-trips through min/max', async () => {
    await node.setParameterValues({ weight: { id: 'weight', value: 0.5, normalized: true } });
    const v = await node.getParameterValues(true, 'weight');
    assert.ok(Math.abs(v.weight.value - 0.5) < 1e-9);
    const raw = await node.getParameterValues(false, 'weight');
    assert.ok(Math.abs(raw.weight.value - 0.5) < 1e-9);
  });

  await check('wam-automation event applies parameter', async () => {
    node.port.sent.length = 0;
    node.scheduleEvents({ type: 'wam-automation', time: 0, data: { id: 'weight', value: 0.8 } });
    const msg = node.port.sent.find((m) => m.type === 'avnd/setParam');
    assert.ok(msg);
    assert.equal(msg.value, 0.8);
  });

  await check('getState/setState round-trips parameter values', async () => {
    await node.setParameterValues({ weight: { id: 'weight', value: 0.33 }, mode: { id: 'mode', value: 2 } });
    const st = await node.getState();
    // mutate then restore
    await node.setParameterValues({ weight: { id: 'weight', value: 0.0 } });
    await node.setState(st);
    const v = await node.getParameterValues(false);
    assert.ok(Math.abs(v.weight.value - 0.33) < 1e-9);
    assert.equal(v.mode.value, 2);
  });
}

// --- Real built-module mapping (if available) -------------------------------

async function testRealModule(cname, asserts) {
  const mjs = join(MODULES_DIR, `${cname}.mjs`);
  if (!existsSync(mjs)) {
    console.log(`  skip ${cname} (no built module at ${mjs})`);
    return;
  }
  const { default: Factory } = await import(pathToFileURL(mjs).href);
  const M = await Factory();
  const Cls = M[cname];
  assert.ok(Cls, `Embind class ${cname} present`);
  const inst = new Cls();
  const n = inst.getParameterCount();
  const infos = [];
  for (let i = 0; i < n; i++) infos.push(inst.getParameterInfo(i));
  if (inst.delete) inst.delete();

  const { info, order, byId } = buildWamParameterInfo(infos);

  // Generic invariants on every produced WAM parameter.
  for (const id of order) {
    const w = info[id];
    assert.ok(['float', 'int', 'boolean', 'choice'].includes(w.type), `${id} valid type`);
    assert.ok(w.maxValue >= w.minValue, `${id} max>=min`);
    assert.ok(w.defaultValue >= w.minValue - 1e-6 && w.defaultValue <= w.maxValue + 1e-6, `${id} default in range`);
    assert.ok(typeof byId[id].avndIndex === 'number', `${id} has routing`);
    if (w.type === 'choice') assert.ok(Array.isArray(w.choices), `${id} choices array`);
  }

  asserts({ infos, info, order, byId });
  console.log(`  ok   ${cname}: ${n} avnd params -> ${order.length} WAM params`);
}

async function testReal() {
  console.log(`== real built modules (${MODULES_DIR}) ==`);

  await testRealModule('avnd_lowpass', ({ info, order }) => {
    // Lowpass has exactly one parameter: Weight float 0..1 init .5.
    assert.equal(order.length, 1);
    const w = info[order[0]];
    assert.equal(w.type, 'float');
    assert.equal(w.minValue, 0);
    assert.equal(w.maxValue, 1);
    assert.ok(Math.abs(w.defaultValue - 0.5) < 1e-6);
  });

  await testRealModule('avnd_wasm_widget_types', ({ info, order, infos }) => {
    const byType = {};
    for (const id of order) byType[info[id].type] = (byType[info[id].type] || 0) + 1;
    // Expect at least one of each scalar WAM type from the widget matrix.
    assert.ok(byType.float > 0, 'has float params');
    assert.ok(byType.int > 0, 'has int params (Count)');
    assert.ok(byType.boolean > 0, 'has boolean (Bypass)');
    assert.ok(byType.choice > 0, 'has choice (Mode/Combo)');
    // Mode enum -> choice with the 3 enum names.
    const mode = Object.values(info).find((w) => w.type === 'choice' && w.choices.includes('Low'));
    if (mode) assert.deepEqual(mode.choices, ['Low', 'Mid', 'High']);
    // rgba -> components ids like "<identifier>/r"; match the real prefix.
    const colorInfo = infos.find((i) => i.type === 'rgba' || i.type === 'rgb');
    if (colorInfo) {
      const prefix = (colorInfo.identifier || '') + '/';
      const colorComps = order.filter((id) => id.startsWith(prefix));
      assert.ok(
        colorComps.length >= 3,
        `color expanded to components (got ${colorComps.length}: ${colorComps.join(',')})`);
    }
  });

  await testRealModule('avnd_all_ports_types', () => {
    /* generic invariants only */
  });
}

await testNode();
await testReal();
await Promise.all(_pending);

console.log('');
if (failures) {
  console.error(`${failures} CHECK(S) FAILED`);
  process.exit(1);
} else {
  console.log('ALL WAM PARAM-MAPPING TESTS PASSED');
}
