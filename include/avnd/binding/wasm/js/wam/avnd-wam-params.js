/* SPDX-License-Identifier: GPL-3.0-or-later */
// Avendish parameter descriptor -> WAM2 WamParameterInfo mapping. Framework-free
// so it unit-tests in plain Node; used by both node.js and processor.js.

// Map one Avendish descriptor to zero or more WamParameterInfo-shaped objects:
// scalars map 1:1, compound (xy/xyz/xyzw/rgb/rgba) expand to one per component,
// opaque types (string/list/...) are skipped. Returned objects carry _avndIndex
// and (for compound) _component for routing.
export function avndParamToWam(info) {
  if (!info) return [];
  const type = info.type;
  const widget = info.widget;
  const baseId = info.identifier || `param${info.index}`;
  const baseLabel = info.name || baseId;
  const units = info.unit || '';

  // --- discrete: enum / combobox -> WAM 'choice' ---------------------------
  if (type === 'enum' || (Array.isArray(info.values) && info.values.length)) {
    const choices = Array.isArray(info.values) ? info.values.slice() : [];
    const min = typeof info.min === 'number' ? info.min : 0;
    const max =
      typeof info.max === 'number' ? info.max : Math.max(0, choices.length - 1);
    const def = clampNum(info.init, min, max, min);
    return [
      {
        id: baseId,
        label: baseLabel,
        type: 'choice',
        defaultValue: def,
        minValue: min,
        maxValue: max,
        discreteStep: 1,
        exponent: 1,
        choices,
        units,
        _avndIndex: info.index,
      },
    ];
  }

  // --- boolean / toggle -> WAM 'boolean' -----------------------------------
  if (type === 'bool' || widget === 'toggle') {
    const def = info.init ? 1 : 0;
    return [
      {
        id: baseId,
        label: baseLabel,
        type: 'boolean',
        defaultValue: def,
        minValue: 0,
        maxValue: 1,
        discreteStep: 1,
        exponent: 1,
        choices: [],
        units,
        _avndIndex: info.index,
      },
    ];
  }

  // --- integer scalar -> WAM 'int' -----------------------------------------
  if (type === 'int') {
    const min = numOr(info.min, 0);
    const max = numOr(info.max, 1);
    const def = clampNum(info.init, min, max, min);
    return [
      {
        id: baseId,
        label: baseLabel,
        type: 'int',
        defaultValue: def,
        minValue: min,
        maxValue: max,
        discreteStep: numOr(info.step, 1),
        exponent: 1,
        choices: [],
        units,
        _avndIndex: info.index,
      },
    ];
  }

  // --- float scalar -> WAM 'float' -----------------------------------------
  if (type === 'float') {
    const min = numOr(info.min, 0);
    const max = numOr(info.max, 1);
    const def = clampNum(info.init, min, max, min);
    return [
      {
        id: baseId,
        label: baseLabel,
        type: 'float',
        defaultValue: def,
        minValue: min,
        maxValue: max,
        discreteStep: typeof info.step === 'number' ? info.step : 0, // 0 = continuous
        exponent: 1,
        choices: [],
        units,
        _avndIndex: info.index,
      },
    ];
  }

  // --- compound numeric (xy/xyz/xyzw/rgb/rgba) -> one float per component ---
  if (
    type === 'xy' ||
    type === 'xyz' ||
    type === 'xyzw' ||
    type === 'rgb' ||
    type === 'rgba'
  ) {
    const n =
      typeof info.components === 'number' && info.components > 1
        ? info.components
        : componentCountForType(type);
    const isColor = type === 'rgb' || type === 'rgba';
    // Color components normalized [0;1]; spatial ones use the declared range.
    const min = isColor ? 0 : numOr(info.min, 0);
    const max = isColor ? 1 : numOr(info.max, 1);
    const def = clampNum(info.init, min, max, isColor ? 0 : min);
    const suffixes = componentSuffixes(type, n);
    const out = [];
    for (let c = 0; c < n; c++) {
      out.push({
        id: `${baseId}/${suffixes[c]}`,
        label: `${baseLabel} ${suffixes[c].toUpperCase()}`,
        type: 'float',
        defaultValue: def,
        minValue: min,
        maxValue: max,
        discreteStep: 0,
        exponent: 1,
        choices: [],
        units,
        _avndIndex: info.index,
        _component: c,
        _componentCount: n,
      });
    }
    return out;
  }

  // string / list / map / variant / ...: no scalar WAM parameter.
  return [];
}

// Build { info: WamParameterInfoMap, order: id[], byId: id->routing } from descriptors.
export function buildWamParameterInfo(avndInfos) {
  const info = {};
  const order = [];
  const byId = {};
  for (const a of avndInfos) {
    for (const w of avndParamToWam(a)) {
      const { _avndIndex, _component, _componentCount, ...pub } = w;
      info[pub.id] = pub;
      order.push(pub.id);
      byId[pub.id] = {
        avndIndex: _avndIndex,
        component: typeof _component === 'number' ? _component : -1,
        componentCount:
          typeof _componentCount === 'number' ? _componentCount : 1,
      };
    }
  }
  return { info, order, byId };
}

// --- helpers ----------------------------------------------------------------

function numOr(v, d) {
  return typeof v === 'number' && Number.isFinite(v) ? v : d;
}

function clampNum(v, min, max, d) {
  const n = numOr(v, d);
  if (n < min) return min;
  if (n > max) return max;
  return n;
}

function componentCountForType(type) {
  switch (type) {
    case 'xy':
      return 2;
    case 'xyz':
    case 'rgb':
      return 3;
    case 'xyzw':
    case 'rgba':
      return 4;
    default:
      return 1;
  }
}

function componentSuffixes(type, n) {
  if (type === 'rgb') return ['r', 'g', 'b'];
  if (type === 'rgba') return ['r', 'g', 'b', 'a'];
  const xyzw = ['x', 'y', 'z', 'w'];
  return xyzw.slice(0, n);
}

export default { avndParamToWam, buildWamParameterInfo };
