import assert from 'node:assert';

// ---- MIDI passthrough ----
{
  const Module = (await import('./avnd_helpers_midi.mjs')).default;
  const M = await Module();
  const p = new M.avnd_helpers_midi();
  p.prepare(0,0,128,48000);
  assert.equal(p.getMidiInputCount(), 1, 'one midi in');
  assert.equal(p.getMidiOutputCount(), 1, 'one midi out');
  p.beginBlock();
  p.pushMidi(0, { bytes:[0x90,60,100], timestamp:10 });
  p.pushMidi(0, { bytes:[0x80,60,0], timestamp:64 });
  const inPtr=M._malloc(4), outPtr=M._malloc(4);
  p.process(inPtr,outPtr,128);
  const out = p.drainMidi();
  M._free(inPtr); M._free(outPtr);
  assert.equal(out.length, 2, 'two messages passed through');
  assert.deepEqual(out[0].bytes, [144,60,100], 'note-on bytes');
  assert.equal(out[0].timestamp, 10, 'note-on ts');
  assert.deepEqual(out[1].bytes, [128,60,0], 'note-off bytes');
  assert.equal(out[1].timestamp, 64, 'note-off ts');
  p.delete();
  console.log('  ok  MIDI passthrough (2 msgs, bytes+timestamps preserved)');
}

// ---- Messages dispatch by name ----
{
  const Module = (await import('./avnd_helpers_messages.mjs')).default;
  const M = await Module();
  const p = new M.avnd_helpers_messages();
  p.prepare(0,0,128,48000);
  assert.equal(p.getMessageCount(), 9, '9 messages');
  assert.equal(p.sendMessage('example', []), true, 'example() ran');
  assert.equal(p.sendMessage('example2', [3.14]), true, 'example2(float) ran');
  assert.equal(p.sendMessage('my_message', []), true, 'my_message ran');
  assert.equal(p.sendMessage('does_not_exist', []), false, 'unknown msg returns false');
  // wrong arg count should not run
  assert.equal(p.sendMessage('example2', []), false, 'example2 w/ 0 args rejected');
  p.delete();
  console.log('  ok  Messages dispatch by name (9 msgs, arg-count checked)');
}

console.log('PHASE 3 MIDI+MESSAGES: ALL OK');
