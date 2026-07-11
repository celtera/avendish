// avnd_max_driver.js  --  in-Max golden-differential driver.
//
// Loaded by avnd_max_driver.maxpat (loadbang -> "run"). For every golden object
// (data staged in avnd_max_data.js by run_max_golden.py) it:
//   * control objects: instantiates the message_processor external, wires each
//     output outlet through [prepend cap<i>] back into this js inlet, replays
//     the golden input controls (numeric + string by name, unnamed ports by
//     proxy inlet), impulse engagements, message-port invocations, and captures
//     every committed output value / callback firing (ordered event lists);
//   * audio objects: builds count~ -> index~(avin<ch>) -> mc.pack~ -> object ->
//     mc.unpack~ -> record~(avout<ch>), fills the input buffers with the golden
//     samples, runs one real DSP block, reads the recorded output samples, and
//     flushes the non-audio outlets with a post-block bang (analyzers);
//   * texture objects: jit.matrix MOP -- feeds filters a deterministic char
//     matrix, bangs to cook, reads the output matrix dims through a named
//     [jit.matrix @adapt 1];
//   * none: instantiate-only smoke (nothing feasible to capture).
// Results are written one JSON line per object to AVND_CFG.report; a per-object
// breadcrumb (AVND_CFG.breadcrumb) lets the Python harness relaunch past a
// crasher and resume (objects already in the report are skipped).
//
// Everything is deliberately async/Task-driven with a delay between "build" and
// "read" so committed/recorded values have settled regardless of Max's
// synchronous-vs-deferred message timing.

autowatch = 0;
inlets = 1;
outlets = 1;

include("avnd_max_config.js"); // -> AVND_CFG {report, breadcrumb, goldenDir}
include("avnd_max_data.js");   // -> AVND_DATA [{name, kind, cases:[...]}]

var FRAMES = 64;
var RATE = 44100;
var BUFSAMPS = 64;   // == FRAMES: record~ (loop off) auto-stops after one block
var CHANS = 2;

var allLines = [];      // raw report lines (prior + new), rewritten each object
var doneNames = {};     // objects already reported (resume after crash)
var WORK = [];          // flattened [{oi, ci, first, last}]
var wi = -1;            // current work index
var cur = [];           // graph objects to clean up for the current case
var curObj = null;      // the object under test for the current case
var capture = {};       // cap<i>/dsptime/... -> last value
var events = {};        // cap<i> -> [[args...], ...] (every firing, in order)
var objCases = [];      // accumulated case-result strings for the current object
var TASKS = [];         // keep Task refs alive
var dspWorks = false;   // set by the DSP self-test: does audio actually render?
var adDriver = null;    // [adstatus driver] (persistent) for set + readback
var dsptimeObj = null;  // [dsptime~] (persistent): samples rendered since dsp start
var setupObjs = [];     // persistent NRT-setup objects (never cleaned up)
var pollTarget = 0, pollThen = "", pollAttempts = 0;  // dsptime~ render gate

function post_(s) { post(s + "\n"); }

// ---- tiny JSON serialization (we only ever emit, never parse, JSON) --------
function jnum(x) {
  if (x === null || x === undefined) return "0";
  var n = Number(x);
  if (!isFinite(n)) return "0";
  return "" + n;
}
function jstr(s) {
  s = "" + s;
  s = s.replace(/\\/g, "\\\\").replace(/"/g, "\\\"");
  return "\"" + s + "\"";
}
function jarr(a) {
  var p = [];
  for (var i = 0; i < a.length; i++) p.push(jnum(a[i]));
  return "[" + p.join(",") + "]";
}
function jval(x) { return (typeof x === "number") ? jnum(x) : jstr(x); }
function jevents(a) {  // [[atoms...], ...] -> JSON
  var evs = [];
  for (var i = 0; i < a.length; i++) {
    var p = [];
    for (var j = 0; j < a[i].length; j++) p.push(jval(a[i][j]));
    evs.push("[" + p.join(",") + "]");
  }
  return "[" + evs.join(",") + "]";
}
// The captured outlet events, keyed by cap index, for the current case.
function jcaps(nCaps) {
  var parts = [];
  for (var i = 0; i < nCaps; i++) {
    var evs = events["cap" + i];
    if (evs && evs.length > 0)
      parts.push(jstr("" + i) + ":" + jevents(evs));
  }
  return "{" + parts.join(",") + "}";
}

// ---- files ----------------------------------------------------------------
function crumb(s) {
  // Max's File "write" mode overwrites in place WITHOUT truncating, so a shorter
  // string leaves a stale tail; pad to a fixed width so the reader (which
  // .strip()s) always sees exactly `s`.
  while (s.length < 240) s += " ";
  var f = new File(AVND_CFG.breadcrumb, "write");
  if (f.isopen) { f.writestring(s); f.close(); }
}
function loadExisting() {
  var f = new File(AVND_CFG.report, "read");
  if (!f.isopen) return;
  while (!f.eof) {
    var ln = f.readline(2000000);
    if (ln && ln.length > 0) {
      allLines.push(ln);
      var m = ln.indexOf("\"name\":\"");
      if (m >= 0) {
        var s = m + 8, e = ln.indexOf("\"", s);
        if (e > s) doneNames[ln.substring(s, e)] = 1;
      }
    }
  }
  f.close();
}
function writeReport() {
  var f = new File(AVND_CFG.report, "write");
  if (!f.isopen) { post_("ERR cannot open report for write"); return; }
  for (var i = 0; i < allLines.length; i++) f.writeline(allLines[i]);
  f.close();
}

// ---- scheduling ------------------------------------------------------------
// Max js Task callbacks don't reliably capture closures across engine versions,
// so dispatch by mode through a stable named callback + Task.arguments.
function defer(mode, ms) {
  var t = new Task(taskCallback, this);
  t.arguments = [mode];
  t.schedule(ms);
  TASKS.push(t);
  if (TASKS.length > 64) TASKS.splice(0, 32);
}
function taskCallback(mode) {
  if (mode === "readStep") readStep();
  else if (mode === "buildStep") buildStep();
  else if (mode === "selfTest") selfTestDSP();
  else if (mode === "selfTestRead") selfTestRead();
  else if (mode === "poll") pollStep();
}

// ---- Non-Real-Time DSP: render MSP with no physical audio device -----------
// Max has no headless mode, but its NonRealTime audio driver computes the DSP
// graph on demand (no hardware). Required companions (else the scheduler --
// which drives our poll Tasks / metro / dsptime~ -- does not advance under NRT):
// Overdrive ON and Scheduler-in-Audio-Interrupt ON. We also pin the sample rate
// and signal vector size to the golden's (64 frames @ 44100) so block-based
// objects see exactly the oracle's one-block buffer.
function setupAudioNRT() {
  var p = this.patcher;
  var X = 660;
  stopDSP();
  try { messnamed("max", "overdrive", 1); } catch (e) {}
  var over = p.newdefault(X, 300, "adstatus", "overdrive"); over.message(1);
  var sched = p.newdefault(X, 326, "adstatus", "scheduler"); sched.message(1); // SIAI
  adDriver = p.newdefault(X, 352, "adstatus", "driver");
  var adp = p.newdefault(X + 170, 352, "prepend", "curdrv");
  p.connect(adDriver, 0, adp, 0);
  p.connect(adp, 0, p.getnamed("driver"), 0);
  adDriver.message("NonRealTime");                    // set the driver
  try { messnamed("dsp", "setdriver", "NonRealTime"); } catch (e) {}
  var sr = p.newdefault(X, 378, "adstatus", "sr"); sr.message(RATE);
  var vs = p.newdefault(X, 404, "adstatus", "sigvs"); vs.message(FRAMES);
  var io = p.newdefault(X, 430, "adstatus", "iovs"); io.message(FRAMES);
  dsptimeObj = p.newdefault(X, 456, "dsptime~");
  var dtp = p.newdefault(X + 170, 456, "prepend", "dsptime");
  p.connect(dsptimeObj, 0, dtp, 0);
  p.connect(dtp, 0, p.getnamed("driver"), 0);
  setupObjs = [over, sched, adDriver, adp, sr, vs, io, dsptimeObj, dtp];
}
function startDSP() {
  var ds = this.patcher.getnamed("dspstart");
  if (ds) ds.message("bang");
  try { messnamed("dsp", "start"); } catch (e) {}
}
function stopDSP() {
  var ds = this.patcher.getnamed("dspstop");
  if (ds) ds.message("bang");
  try { messnamed("dsp", "stop"); } catch (e) {}
}
// Gate on dsptime~ (samples rendered), NOT wall-clock: NRT free-runs, so poll
// until at least `target` samples have elapsed, then dispatch `then`.
function startPoll(target, then) {
  pollTarget = target; pollThen = then; pollAttempts = 0;
  defer("poll", 8);
}
function pollStep() {
  pollAttempts++;
  if (dsptimeObj) dsptimeObj.message("bang");
  var t = capture["dsptime"];
  if ((typeof t === "number" && t >= pollTarget) || pollAttempts > 120) {
    taskCallback(pollThen);
    return;
  }
  defer("poll", 8);
}

// One-shot: prove NRT DSP + record~ actually capture a signal here. Sets the
// global dspWorks flag; the per-object audio path only activates when true.
function selfTestDSP() {
  var p = this.patcher;
  capture = {};
  var b = new Buffer("avout0");
  for (var z = 0; z < BUFSAMPS; z++) b.poke(1, z, 0.0);
  if (adDriver) adDriver.message("bang");             // read back the driver name
  var osc = p.newdefault(40, 520, "cycle~", 441);
  var snap = p.newdefault(40, 545, "snapshot~", 5);
  var pre = p.newdefault(200, 545, "prepend", "stpeak");
  var rec = p.newdefault(40, 580, "record~", "avout0");
  cur = [osc, snap, pre, rec];
  p.connect(osc, 0, snap, 0);
  p.connect(snap, 0, pre, 0);
  p.connect(pre, 0, this.patcher.getnamed("driver"), 0);
  p.connect(osc, 0, rec, 0);
  rec.message(1);
  startDSP();
  startPoll(FRAMES, "selfTestRead");
}
function selfTestRead() {
  var b = new Buffer("avout0");
  var peak = 0.0;
  for (var i = 0; i < BUFSAMPS; i++) {
    var v = b.peek(1, i);
    if (v < 0) v = -v;
    if (v > peak) peak = v;
  }
  stopDSP();
  var snap = capture["stpeak"];
  dspWorks = (peak > 0.0) || (typeof snap === "number" && snap !== 0.0);
  var dbg = new File(AVND_CFG.breadcrumb + ".dbg", "readwrite");
  if (dbg.isopen) {
    dbg.position = dbg.eof;
    dbg.writeline("dsp_works=" + dspWorks);
    dbg.writeline("audio_driver=" + (capture["curdrv"] === undefined ? "none" : capture["curdrv"]));
    dbg.writeline("selftest_record_peak=" + peak);
    dbg.writeline("selftest_dsptime=" + (capture["dsptime"] === undefined ? "none" : capture["dsptime"]));
    dbg.close();
  }
  cleanup();
  buildStep();
}

// ---- entry -----------------------------------------------------------------
function run() {
  loadExisting();
  WORK = [];
  for (var oi = 0; oi < AVND_DATA.length; oi++) {
    var o = AVND_DATA[oi];
    if (doneNames[o.name]) continue;
    var nc = o.cases.length;
    for (var ci = 0; ci < nc; ci++)
      WORK.push({ oi: oi, ci: ci, first: ci === 0, last: ci === nc - 1 });
  }
  post_("avnd max driver: " + WORK.length + " cases over "
        + (AVND_DATA.length - countKeys(doneNames)) + " objects");
  wi = -1;
  // size the static capture buffers once.
  for (var ch = 0; ch < CHANS; ch++) {
    sizeBuf("buf_avin" + ch);
    sizeBuf("buf_avout" + ch);
  }
  // one-shot debug: which static boxes did we resolve by scripting name?
  var dbg = new File(AVND_CFG.breadcrumb + ".dbg", "write");
  if (dbg.isopen) {
    dbg.writeline("driver=" + (this.patcher.getnamed("driver") ? "ok" : "NULL"));
    dbg.writeline("dspstart=" + (this.patcher.getnamed("dspstart") ? "ok" : "NULL"));
    dbg.writeline("dspstop=" + (this.patcher.getnamed("dspstop") ? "ok" : "NULL"));
    dbg.writeline("buf_avout0=" + (this.patcher.getnamed("buf_avout0") ? "ok" : "NULL"));
    var tb = new Buffer("avout0");
    dbg.writeline("avout0.framecount=" + tb.framecount());
    dbg.close();
  }
  setupAudioNRT();
  // give the driver switch a moment to apply before the self-test starts DSP.
  defer("selfTest", 300);
}
function countKeys(o) { var n = 0; for (var k in o) n++; return n; }
function sizeBuf(varname) {
  var b = this.patcher.getnamed(varname);
  if (b) b.message("sizeinsamps", BUFSAMPS);
}

// ---- per-case build --------------------------------------------------------
function buildStep() {
  wi++;
  if (wi >= WORK.length) { finalize(); return; }
  var w = WORK[wi];
  var obj = AVND_DATA[w.oi];
  crumb(obj.name + ":case" + w.ci);
  if (w.first) objCases = [];
  capture = {};
  events = {};
  cur = [];
  curObj = null;

  var kind = obj.kind;
  var ok = false;
  try {
    if (kind === "audio") ok = buildAudio(obj, obj.cases[w.ci]);
    else if (kind === "control") ok = buildControl(obj, obj.cases[w.ci]);
    else if (kind === "texture") ok = buildTexture(obj, obj.cases[w.ci]);
    else ok = buildNone(obj, obj.cases[w.ci]);
  } catch (e) {
    ok = false;
  }
  if (!ok) {
    objCases.push("{\"index\":" + w.ci + ",\"error\":\"could not instantiate\"}");
    cleanup();
    if (w.last) commitObject(obj);
    defer("buildStep", 20);
    return;
  }
  if (kind === "audio" && dspWorks)
    startPoll(FRAMES, "readStep");   // gate the read on dsptime~ (samples rendered)
  else
    defer("readStep", 40);
}

// Wire the object's message outlets (capBase..capBase+caps-1) back into this
// js through [prepend cap<i>], so every firing is captured into `events`.
function wireCaps(p, o, obj) {
  var driver = p.getnamed("driver");
  var n = obj.caps.length;
  for (var i = 0; i < n; i++) {
    var pre = p.newdefault(220 + i * 60, 430, "prepend", "cap" + i);
    cur.push(pre);
    p.connect(o, obj.capBase + i, pre, 0);
    p.connect(pre, 0, driver, 0);
  }
}

// Replay the case's string+numeric controls (by name -- works for every named
// port on inlet 0 without triggering processing in the audio/jitter bindings),
// impulse engagements (selector-only message) and message invocations.
function sendNamedInputs(o, c) {
  for (var k = 0; k < c.controls.length; k++) {
    var nm = c.controls[k][1], val = c.controls[k][2];
    if (nm && nm.length > 0) o.message(nm, val);
  }
  for (var i = 0; i < c.impulses.length; i++)
    if (c.impulses[i] && c.impulses[i].length > 0)
      o.message(c.impulses[i]);
  for (var m = 0; m < c.messages.length; m++)
    sendMessagePort(o, c.messages[m]);
}

// jsobj.message is a host method: .apply() on it is not reliable across Max
// versions, so dispatch by arity (declared message ports have few args).
function sendMessagePort(o, msg) {
  var nm = msg[0], ar = msg[1];
  if (!nm || nm.length === 0) return;
  if (ar.length === 0) o.message(nm);
  else if (ar.length === 1) o.message(nm, ar[0]);
  else if (ar.length === 2) o.message(nm, ar[0], ar[1]);
  else if (ar.length === 3) o.message(nm, ar[0], ar[1], ar[2]);
  else o.message(nm, ar[0], ar[1], ar[2], ar[3]);
}

function buildControl(obj, c) {
  var p = this.patcher;
  var o = p.newdefault(220, 300, obj.name);
  if (!o) return false;
  cur.push(o);
  curObj = o;
  wireCaps(p, o, obj);
  // Named ports: a "<name> <value>" message on inlet 0 sets the parameter
  // WITHOUT triggering processing. Unnamed ports (positional "p<i>" golden
  // placeholder) have no name to match in the Max binding: feed the value to
  // proxy inlet i through a message box instead (the message_processor maps
  // proxy inlet i -> parameter i; inlet 0 also triggers process()+commit).
  var msgs = [];
  for (var k = 0; k < c.controls.length; k++) {
    var idx = c.controls[k][0], nm = c.controls[k][1], val = c.controls[k][2];
    // Container controls (xy / rgba / array / list) arrive as a JS array:
    // spread the components into a list message so every component lands.
    var atoms = (val instanceof Array) ? val : [val];
    if (nm && nm.length > 0 && !(/^p\d+$/.test(nm))) {
      o.message.apply(o, [nm].concat(atoms));
    } else {
      var mb = p.newdefault(20 + k * 70, 230, "message");
      mb.message.apply(mb, ["set"].concat(atoms)); // content not from a ctor arg
      cur.push(mb);
      p.connect(mb, 0, o, idx);
      msgs.push([mb, idx]);
    }
  }
  for (var m = msgs.length - 1; m >= 0; m--)
    if (msgs[m][1] !== 0) msgs[m][0].message("bang");
  for (var m2 = 0; m2 < msgs.length; m2++)
    if (msgs[m2][1] === 0) msgs[m2][0].message("bang");
  // Impulse engagements + declared message-port invocations, then the final
  // process()+commit.
  for (var i = 0; i < c.impulses.length; i++)
    if (c.impulses[i] && c.impulses[i].length > 0)
      o.message(c.impulses[i]);
  for (var mm = 0; mm < c.messages.length; mm++)
    sendMessagePort(o, c.messages[mm]);
  o.message("bang"); // force a final process()+commit on inlet 0
  return true;
}

function buildTexture(obj, c) {
  var p = this.patcher;
  var o = p.newdefault(220, 300, obj.name);
  if (!o) return false;
  cur.push(o);
  curObj = o;
  // Receiving matrix adapts to whatever the MOP outputs (dims + planes).
  var mout = p.newdefault(220, 430, "jit.matrix", "avndtexout", "@adapt", 1);
  cur.push(mout);
  p.connect(o, 0, mout, 0);
  // reset the receiver to a 1x1 so stale dims from a previous case can never
  // masquerade as this case's output.
  mout.message("dim", 1, 1);
  // Texture filters: feed a deterministic char 4-plane matrix of the golden's
  // recorded input size (content is informational only -- Jitter plane order
  // is ARGB vs the object's RGBA, and the dims diff is what's authoritative).
  if (c.texIn && c.texIn.length > 0) {
    var w = c.texIn[0][0], h = c.texIn[0][1];
    var jin = new JitterMatrix("avndtexin", 4, "char", w, h);
    for (var y = 0; y < h; y++)
      for (var x = 0; x < w; x++) {
        var base = (y * w + x) * 4;
        jin.setcell2d(x, y, base % 256, (base + 1) % 256, (base + 2) % 256,
                      (base + 3) % 256);
      }
  }
  // Controls (e.g. a generator's Width/Height) by name -- no processing yet.
  sendNamedInputs(o, c);
  if (c.texIn && c.texIn.length > 0)
    o.message("jit_matrix", "avndtexin");
  o.message("bang"); // cook: matrix_calc + outputmatrix + commit
  return true;
}

function buildAudio(obj, c) {
  var p = this.patcher;
  var o = p.newdefault(420, 300, obj.name);
  if (!o) return false;
  cur.push(o);
  curObj = o;
  // If DSP can't render in this session (no audio driver -- Max has no headless
  // mode), instantiating the audio external is all we can verify. Skip the futile
  // signal graph; readAudio will report dsp:false so it isn't a false MISMATCH.
  if (!dspWorks) return true;
  wireCaps(p, o, obj);   // control/callback outlets right of the signal outlet
  var nin = c.audioIn.length;
  var nout = c.nAudioOut;  // 0 for analyzers: nothing to record~, controls only
  if (nout > CHANS) nout = CHANS;

  if (nin > 0) {
    // count~ only advances while its left inlet carries a NONZERO signal; with
    // nothing connected its gate is 0 and it stays stuck at index 0 (the object
    // would see a constant input). Feed it a constant 1 so it counts 0,1,2,...
    // Wrap at FRAMES so every block replays the same golden input (the signal
    // vector is pinned to FRAMES, so each block is exactly one pass) -- like
    // Pd's repeating [tabreceive~]; otherwise index~ runs past the buffer and
    // control outputs committed after the first block reflect garbage input.
    var one = p.newdefault(20, 110, "sig~", 1);
    cur.push(one);
    var cnt = p.newdefault(50, 140, "count~", 0, FRAMES);
    cur.push(cnt);
    p.connect(one, 0, cnt, 0);
    var pack = (nin > 1) ? p.newdefault(300, 240, "mc.pack~", nin) : null;
    if (pack) cur.push(pack);
    for (var ch = 0; ch < nin && ch < CHANS; ch++) {
      var b = new Buffer("avin" + ch);
      var src = c.audioIn[ch];
      for (var i = 0; i < FRAMES && i < src.length; i++) b.poke(1, i, src[i]);
      for (var j = src.length; j < BUFSAMPS; j++) b.poke(1, j, 0.0);
      var idx = p.newdefault(50, 190 + ch * 40, "index~", "avin" + ch);
      cur.push(idx);
      p.connect(cnt, 0, idx, 0);
      if (pack) p.connect(idx, 0, pack, ch);
      else p.connect(idx, 0, o, 0);
    }
    if (pack) p.connect(pack, 0, o, 0);
  }
  // audio_processor has only the signal inlet 0 and matches controls by NAME
  // (no proxy inlets / index fallback): strings, impulses and message ports
  // included.
  sendNamedInputs(o, c);

  var unpack = (nout > 1) ? p.newdefault(420, 400, "mc.unpack~", nout) : null;
  if (unpack) { cur.push(unpack); p.connect(o, 0, unpack, 0); }
  for (var oc = 0; oc < nout; oc++) {
    var bo = new Buffer("avout" + oc);
    for (var z = 0; z < BUFSAMPS; z++) bo.poke(1, z, 0.0); // clear stale samples
    var rec = p.newdefault(420 + oc * 80, 470, "record~", "avout" + oc);
    cur.push(rec);
    if (unpack) p.connect(unpack, oc, rec, 0);
    else p.connect(o, 0, rec, 0);
    rec.message(1); // arm: begins recording the moment DSP starts
  }
  var ds = p.getnamed("dspstart");
  if (ds) ds.message("bang");
  return true;
}

function buildNone(obj, c) {
  var o = this.patcher.newdefault(220, 300, obj.name);
  if (!o) return false;
  cur.push(o);
  return true;
}

// ---- per-case read ---------------------------------------------------------
function readStep() {
  var w = WORK[wi];
  var obj = AVND_DATA[w.oi];
  var c = obj.cases[w.ci];
  var line;
  try {
    if (obj.kind === "audio") line = readAudio(obj, c, w.ci);
    else if (obj.kind === "control") line = readControl(obj, c, w.ci);
    else if (obj.kind === "texture") line = readTexture(obj, c, w.ci);
    else line = "{\"index\":" + w.ci + ",\"instantiated\":true}";
  } catch (e) {
    line = "{\"index\":" + w.ci + ",\"error\":\"read failed\"}";
  }
  objCases.push(line);
  cleanup();
  if (w.last) commitObject(obj);
  defer("buildStep", 20);
}

function readControl(obj, c, ci) {
  return "{\"index\":" + ci + ",\"caps\":" + jcaps(obj.caps.length) + "}";
}

function readTexture(obj, c, ci) {
  var jm = new JitterMatrix("avndtexout");
  var dim = jm.dim;
  var w = 0, h = 1;
  if (dim instanceof Array) { w = dim[0]; if (dim.length > 1) h = dim[1]; }
  else w = dim;
  return "{\"index\":" + ci + ",\"texture\":[{\"width\":" + jnum(w)
       + ",\"height\":" + jnum(h) + ",\"planes\":" + jnum(jm.planecount)
       + "}],\"caps\":" + jcaps(obj.caps.length) + "}";
}

function readAudio(obj, c, ci) {
  if (!dspWorks)
    return "{\"index\":" + ci + ",\"instantiated\":true,\"dsp\":false}";
  // Flush the non-audio outputs computed by the rendered block(s): the audio
  // binding commits them on bang (analyzers like avnd_peak).
  if (curObj && obj.caps.length > 0) curObj.message("bang");
  var nout = c.nAudioOut;
  if (nout > CHANS) nout = CHANS;
  var ds = this.patcher.getnamed("dspstop");
  var chans = [];
  for (var oc = 0; oc < nout; oc++) {
    var b = new Buffer("avout" + oc);
    var arr = [];
    for (var i = 0; i < FRAMES; i++) {
      var v = b.peek(1, i);
      arr.push((v === undefined || v === null) ? 0.0 : v);
    }
    chans.push(jarr(arr));
  }
  if (ds) ds.message("bang");
  return "{\"index\":" + ci + ",\"audio\":[" + chans.join(",")
       + "],\"caps\":" + jcaps(obj.caps.length) + "}";
}

// ---- capture handler: [prepend cap<i>] -> here -----------------------------
function anything() {
  // Captures everything wired back into inlet 0: control/callback outputs
  // (cap<i>), the DSP self-test signal (stpeak), the audio driver name
  // (curdrv) and the rendered-sample count (dsptime).
  var sel = "" + messagename;
  var a = arrayfromargs(arguments);
  // Prefer the first numeric arg (scalar control / signal value); fall back to
  // the first arg as-is (string/enum/symbol outputs, driver name).
  var v = (a.length > 0) ? a[0] : 0;
  for (var i = 0; i < a.length; i++) {
    if (typeof a[i] === "number") { v = a[i]; break; }
  }
  capture[sel] = v;
  // Full event log per capture outlet (callback diffing needs every firing,
  // in order, with all its arguments).
  if (sel.substring(0, 3) === "cap") {
    if (!events[sel]) events[sel] = [];
    events[sel].push(a.slice(0));
  }
}
function msg_float(v) { /* bare float outputs shouldn't reach us (prepended) */ }
function msg_int(v) { }

// ---- object bookkeeping ----------------------------------------------------
function commitObject(obj) {
  var line = "{\"name\":" + jstr(obj.name) + ",\"kind\":" + jstr(obj.kind)
           + ",\"cases\":[" + objCases.join(",") + "]}";
  allLines.push(line);
  doneNames[obj.name] = 1;
  writeReport();
}
function cleanup() {
  var p = this.patcher;
  for (var i = 0; i < cur.length; i++) {
    try { p.remove(cur[i]); } catch (e) {}
  }
  cur = [];
}
function finalize() {
  crumb("DONE");
  post_("avnd max driver: DONE (" + allLines.length + " objects)");
}
