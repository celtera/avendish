// avnd_max_driver.js  --  in-Max golden-differential driver.
//
// Loaded by avnd_max_driver.maxpat (loadbang -> "run"). For every golden object
// (data staged in avnd_max_data.js by run_max_golden.py) it:
//   * control objects: instantiates the message_processor external, wires each
//     output outlet through [prepend cap<i>] back into this js inlet, replays the
//     golden input controls and captures the committed output values;
//   * audio objects: builds count~ -> index~(avin<ch>) -> mc.pack~ -> object ->
//     mc.unpack~ -> record~(avout<ch>), fills the input buffers with the golden
//     samples, runs one real DSP block and reads the recorded output samples;
//   * none/texture: instantiate-only smoke (nothing feasible to capture).
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
var BUFSAMPS = 128;
var CHANS = 2;

var allLines = [];      // raw report lines (prior + new), rewritten each object
var doneNames = {};     // objects already reported (resume after crash)
var WORK = [];          // flattened [{oi, ci, first, last}]
var wi = -1;            // current work index
var cur = [];           // graph objects to clean up for the current case
var capture = {};       // cap<i> -> value  (control capture, current case)
var objCases = [];      // accumulated case-result strings for the current object
var TASKS = [];         // keep Task refs alive
var dspWorks = false;   // set by the DSP self-test: does audio actually render?

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
  else if (mode === "selfTestRead") selfTestRead();
}

// One-shot: prove that global DSP + record~ actually capture a signal in this
// automated Max session. Writes the peak sample to <breadcrumb>.dbg.
function selfTestDSP() {
  var p = this.patcher;
  capture = {};
  var b = new Buffer("avout0");
  for (var z = 0; z < BUFSAMPS; z++) b.poke(1, z, 0.0);
  // report the current audio driver (adstatus reports on bang).
  var ads = p.newdefault(400, 520, "adstatus", "driver");
  var adp = p.newdefault(600, 520, "prepend", "curdrv");
  p.connect(ads, 0, adp, 0);
  p.connect(adp, 0, p.getnamed("driver"), 0);
  ads.message("bang");
  var osc = p.newdefault(40, 520, "cycle~", 441);
  var snap = p.newdefault(40, 545, "snapshot~", 20);       // live sample every 20ms
  var pre = p.newdefault(200, 545, "prepend", "stpeak");
  var rec = p.newdefault(40, 580, "record~", "avout0");
  cur = [ads, adp, osc, snap, pre, rec];
  p.connect(osc, 0, snap, 0);
  p.connect(snap, 0, pre, 0);
  p.connect(pre, 0, this.patcher.getnamed("driver"), 0);
  p.connect(osc, 0, rec, 0);
  rec.message(1);
  startDSP();
  defer("selfTestRead", 800);
}
var dspDriverSet = false;
function startDSP() {
  // No live audio device is guaranteed in an automated Max, so drive the graph
  // with the Non-Real-Time driver (renders the DSP chain without hardware).
  if (!dspDriverSet) {
    var dv = this.patcher.getnamed("dspdriver");
    if (dv) dv.message("bang");
    try { messnamed("dsp", "driver", "NonRealTime"); } catch (e) {}
    dspDriverSet = true;
  }
  var ds = this.patcher.getnamed("dspstart");
  if (ds) ds.message("bang");
  try { messnamed("dsp", "start"); } catch (e) {}
}
function stopDSP() {
  var ds = this.patcher.getnamed("dspstop");
  if (ds) ds.message("bang");
  try { messnamed("dsp", "stop"); } catch (e) {}
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
    dbg.writeline("selftest_record_peak=" + peak);
    dbg.writeline("selftest_snapshot=" + (capture["stpeak"] === undefined ? "none" : capture["stpeak"]));
    dbg.writeline("audio_driver=" + (capture["curdrv"] === undefined ? "none" : capture["curdrv"]));
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
  selfTestDSP();
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
  cur = [];

  var kind = obj.kind;
  var ok = false;
  try {
    if (kind === "audio") ok = buildAudio(obj, obj.cases[w.ci]);
    else if (kind === "control") ok = buildControl(obj, obj.cases[w.ci]);
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
  defer("readStep", kind === "audio" ? 250 : 50);
}

function buildControl(obj, c) {
  var p = this.patcher;
  var o = p.newdefault(220, 300, obj.name);
  if (!o) return false;
  cur.push(o);
  var driver = p.getnamed("driver");
  var nout = c.outCtrlNames.length;
  for (var i = 0; i < nout; i++) {
    var pre = p.newdefault(220 + i * 60, 400, "prepend", "cap" + i);
    cur.push(pre);
    p.connect(o, i, pre, 0);
    p.connect(pre, 0, driver, 0);
  }
  // Set each explicit control by INLET INDEX, not by name: the message_processor
  // maps proxy inlet i -> parameter i, and its name-matching path fails for
  // unnamed ports (whose golden name is a positional "p<i>" placeholder). A
  // message box feeding inlet i sets exactly parameter i. Inlet 0 also triggers
  // process()+commit, so set the higher inlets first, then inlet 0 / a bang.
  var msgs = [];
  for (var k = 0; k < c.controls.length; k++) {
    var idx = c.controls[k][0], val = c.controls[k][2];
    var mb = p.newdefault(20 + k * 70, 230, "message");
    mb.message("set", val); // the numeric content is NOT taken from a ctor arg
    cur.push(mb);
    p.connect(mb, 0, o, idx);
    msgs.push(mb);
  }
  for (var m = msgs.length - 1; m >= 0; m--) {
    var idx2 = c.controls[m][0];
    if (idx2 !== 0) msgs[m].message("bang");
  }
  for (var m2 = 0; m2 < msgs.length; m2++)
    if (c.controls[m2][0] === 0) msgs[m2].message("bang");
  o.message("bang"); // force a final process()+commit on inlet 0
  return true;
}

function buildAudio(obj, c) {
  var p = this.patcher;
  var o = p.newdefault(420, 300, obj.name);
  if (!o) return false;
  cur.push(o);
  // If DSP can't render in this session (no audio driver -- Max has no headless
  // mode), instantiating the audio external is all we can verify. Skip the futile
  // signal graph; readAudio will report dsp:false so it isn't a false MISMATCH.
  if (!dspWorks) return true;
  var nin = c.audioIn.length;
  var nout = c.nAudioOut > 0 ? c.nAudioOut : nin;
  if (nout < 1) nout = 1;
  if (nout > CHANS) nout = CHANS;

  if (nin > 0) {
    var cnt = p.newdefault(50, 140, "count~");
    cur.push(cnt);
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
  // (no proxy inlets / index fallback), so set them via "<name> <value>".
  for (var k = 0; k < c.controls.length; k++)
    o.message(c.controls[k][1], c.controls[k][2]);

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
  var parts = [];
  var nout = c.outCtrlNames.length;
  for (var i = 0; i < nout; i++) {
    var key = "cap" + i;
    if (capture[key] !== undefined) {
      var nm = c.outCtrlNames[i];
      parts.push(jstr(nm) + ":" + jnum(capture[key]));
    }
  }
  return "{\"index\":" + ci + ",\"controls\":{" + parts.join(",") + "}}";
}

function readAudio(obj, c, ci) {
  if (!dspWorks)
    return "{\"index\":" + ci + ",\"instantiated\":true,\"dsp\":false}";
  var nout = c.nAudioOut > 0 ? c.nAudioOut : c.audioIn.length;
  if (nout < 1) nout = 1;
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
  return "{\"index\":" + ci + ",\"audio\":[" + chans.join(",") + "]}";
}

// ---- capture handler: [prepend cap<i>] -> here -----------------------------
function anything() {
  var sel = "" + messagename;
  if (sel.substring(0, 3) === "cap" || sel === "stpeak" || sel === "curdrv") {
    var a = arrayfromargs(arguments);
    // Prefer the first numeric arg (scalar control / signal value); fall back to
    // the first arg as-is (string/enum/symbol outputs, driver name).
    var v = (a.length > 0) ? a[0] : 0;
    for (var i = 0; i < a.length; i++) {
      if (typeof a[i] === "number") { v = a[i]; break; }
    }
    capture[sel] = v;
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
