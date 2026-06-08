import Module from './oscr_sa_filter.mjs';
const M = await Module();
const p = new M.oscr_sa_filter();
p.prepare(0,0,128,48000);
console.log('params:', p.getParameterCount(), 'outputs:', p.getOutputCount());
for(let i=0;i<p.getParameterCount();i++) console.log('  param', i, JSON.stringify(p.getParameterInfo(i)));
p.beginBlock();
// schedule input "In" value 0.5 at frame 20, value 1.0 at frame 80
p.scheduleControl(0, 20, 0.5);
p.scheduleControl(0, 80, 1.0);
const inPtr=M._malloc(4), outPtr=M._malloc(4);
p.process(inPtr, outPtr, 128);
M._free(inPtr); M._free(outPtr);
const outs = p.getControlOutputs();
console.log('control outputs:', JSON.stringify(outs));
p.delete();
// expected: out has changes at frames 20 and 80
const o0 = outs.find(o=>o.index===0) || outs[0];
const frames = (o0 && o0.changes) ? o0.changes.map(c=>c.frame) : [];
console.log('output change frames:', frames);
console.log(frames.includes(20) && frames.includes(80) ? 'SAMPLE-ACCURATE OK' : 'SAMPLE-ACCURATE FAIL');
