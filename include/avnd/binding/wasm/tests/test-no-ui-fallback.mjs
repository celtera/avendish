// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression: a plug-in with no `ui` member still builds and falls back to a
// flat vbox-of-controls layout; paintCustom() is a no-op.

import Module from './avnd_lowpass.mjs';

let fail = 0;
const c = (b, m) => { console.log((b ? 'ok   ' : 'FAIL ') + m); if (!b) fail++; };

const M = await Module();
const p = new M.avnd_lowpass();
p.prepare(1, 1, 128, 48000);

c(p.hasCustomUI() === false, 'Lowpass hasCustomUI() === false');
c(p.getCustomUICount() === 0, 'Lowpass getCustomUICount() === 0');

const L = p.getUILayout();
console.log('layout:', JSON.stringify(L));
c(L && L.type === 'vbox', 'fallback layout is vbox');
c(Array.isArray(L.children) && L.children.length === p.getParameterCount(),
  'fallback has one control per parameter');
c(L.children.every((n, i) => n.type === 'control' && n.index === i),
  'fallback controls reference parameter indices');

p.paintCustom(0, {});
c(true, 'paintCustom no-op on no-UI plug-in did not throw');

p.delete();
console.log(fail === 0 ? '\nNO-UI-FALLBACK OK' : `\nNO-UI-FALLBACK FAIL (${fail})`);
process.exit(fail ? 1 : 0);
