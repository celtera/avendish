import assert from 'node:assert';
import Module from './avnd_helpers_multichannel.mjs';
const M = await Module();

// default negotiation
{
  const p = new M.avnd_helpers_multichannel();
  p.prepare(2, 2, 128, 48000);
  console.log('default: in', p.inputChannels(), 'out', p.outputChannels());
  p.delete();
}
// Channels=4 before prepare -> 4 out channels
{
  const p = new M.avnd_helpers_multichannel();
  let chIdx=-1;
  for(let i=0;i<p.getParameterCount();i++){ const inf=p.getParameterInfo(i); if(inf.name==='Channels'||inf.identifier==='Channels') chIdx=i; }
  console.log('Channels param idx:', chIdx);
  p.setParameter(chIdx, 4);
  p.prepare(2, 2, 128, 48000);
  const oc = p.outputChannels();
  console.log('after Channels=4 + prepare: out channels =', oc);
  assert.equal(oc, 4, 'negotiated 4 output channels');

  // input ch0 = ramp, expect copied to all 4 out channels
  const frames=128, inCh=p.inputChannels(), outCh=p.outputChannels();
  const inPtr=M._malloc(frames*inCh*4), outPtr=M._malloc(frames*outCh*4);
  const inV=new Float32Array(M.HEAPF32.buffer, inPtr, frames*inCh);
  for(let i=0;i<frames;i++) inV[i]=i*0.01;
  p.beginBlock();
  p.process(inPtr, outPtr, frames);
  // operator returns early until channels match, so process again
  p.process(inPtr, outPtr, frames);
  const outV=new Float32Array(M.HEAPF32.buffer, outPtr, frames*outCh);
  let okCopy=true;
  for(let c=0;c<outCh;c++){ if(Math.abs(outV[c*frames+5]-0.05)>1e-4){ okCopy=false; } }
  console.log('out[ch*frames+5] per channel:', Array.from({length:outCh},(_, c)=>outV[c*frames+5].toFixed(3)));
  M._free(inPtr); M._free(outPtr);
  p.delete();
  assert.ok(okCopy, 'input ch0 copied to all output channels');
  console.log('  ok  multichannel negotiation + processing');
}
console.log('MULTICHANNEL OK');
