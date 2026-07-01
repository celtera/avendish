/* SPDX-License-Identifier: GPL-3.0-or-later */

// Generate help / example patches for an avendish object, for one backend, from
// the object's introspection dump (dump/<c_name>.json produced by the dump
// backend). This is the JSON-driven generalization of GeneratePdHelp.cpp; it is
// invoked per object at build time, mirroring json_to_maxref.
//
//   generate_patches <backend> <input.json> <output-path>
//
// backend ∈ { pd, max, godot, td, python }
//
// Phase 0: scaffolding — the model parser, the per-backend dispatch and minimal
// but valid stub emitters. Phases 1-4 flesh out each emitter (see
// HELP_PATCH_GENERATION_PLAN.md).

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Normalized model: a backend-agnostic view of the dump JSON. The emitters work
// against this rather than poking at the raw JSON, so the dump schema is parsed
// in exactly one place.
// ---------------------------------------------------------------------------
struct range_t
{
  std::optional<double> min, max, init;
};

struct port_t
{
  std::string name;
  std::string description;
  std::string type; // parameter / audio / midi / texture / message / callback / ...

  // type == "parameter"
  std::string value_type; // float / int / bool / string / enum / unknown
  bool is_control = false;
  bool is_value_port = false;
  std::optional<range_t> range;
  std::vector<std::string> choices; // enum
  std::optional<json> default_value;
  std::string widget;

  // type == "audio"
  std::string audio_sample_format; // float / double
  std::string audio_port_format;   // sample / channel / bus
};

struct message_t
{
  std::string name;
  std::vector<std::string> arguments;
  std::string return_type;
};

struct model_t
{
  std::string name;
  std::string c_name;
  std::string uuid;
  std::string category;
  std::string description;
  std::string short_description;
  std::string author;
  std::string version;

  std::vector<port_t> inputs;
  std::vector<port_t> outputs;
  std::vector<message_t> messages;
};

std::string str(const json& o, const char* key, std::string_view fallback = {})
{
  if(auto it = o.find(key); it != o.end() && it->is_string())
    return it->get<std::string>();
  return std::string{fallback};
}

range_t parse_range(const json& r)
{
  range_t out;
  if(auto it = r.find("min"); it != r.end() && it->is_number())
    out.min = it->get<double>();
  if(auto it = r.find("max"); it != r.end() && it->is_number())
    out.max = it->get<double>();
  if(auto it = r.find("init"); it != r.end() && it->is_number())
    out.init = it->get<double>();
  return out;
}

port_t parse_port(const json& p)
{
  port_t out;
  out.name = str(p, "name", "port");
  out.description = str(p, "description");
  out.type = str(p, "type", "unknown");

  if(auto pit = p.find("parameter"); pit != p.end())
  {
    const json& par = *pit;
    out.value_type = str(par, "value_type", "unknown");
    out.is_control = par.value("control", false);
    out.is_value_port = par.value("value_port", false);
    out.widget = str(par, "widget");
    if(auto rit = par.find("range"); rit != par.end())
      out.range = parse_range(*rit);
    if(auto cit = par.find("choices"); cit != par.end() && cit->is_array())
      for(const auto& c : *cit)
        if(c.is_string())
          out.choices.push_back(c.get<std::string>());
    if(auto dit = par.find("default"); dit != par.end())
      out.default_value = *dit;
  }
  if(auto ait = p.find("audio"); ait != p.end())
  {
    out.audio_sample_format = str(*ait, "sample_format");
    out.audio_port_format = str(*ait, "port_format");
  }
  return out;
}

model_t parse_model(const json& j)
{
  model_t m;
  if(auto it = j.find("metadatas"); it != j.end())
  {
    const json& md = *it;
    m.name = str(md, "name", "Object");
    m.c_name = str(md, "c_name", m.name);
    m.uuid = str(md, "uuid");
    m.category = str(md, "category");
    m.description = str(md, "description");
    m.short_description = str(md, "short_description");
    m.author = str(md, "author");
    m.version = str(md, "version");
  }
  if(auto it = j.find("inputs"); it != j.end() && it->is_array())
    for(const auto& p : *it)
      m.inputs.push_back(parse_port(p));
  if(auto it = j.find("outputs"); it != j.end() && it->is_array())
    for(const auto& p : *it)
      m.outputs.push_back(parse_port(p));
  if(auto it = j.find("messages"); it != j.end() && it->is_array())
  {
    for(const auto& msg : *it)
    {
      message_t mm;
      mm.name = str(msg, "name", "message");
      mm.return_type = str(msg, "return");
      if(auto ait = msg.find("arguments"); ait != msg.end() && ait->is_array())
        for(const auto& a : *ait)
          mm.arguments.push_back(a.is_string() ? a.get<std::string>() : a.dump());
      m.messages.push_back(mm);
    }
  }
  return m;
}

std::string blurb(const model_t& m)
{
  if(!m.short_description.empty())
    return m.short_description;
  if(!m.description.empty())
    return m.description;
  return "auto-generated help patch";
}

// ---------------------------------------------------------------------------
// Emitters. Phase 0: minimal but valid output that instantiates the object and
// carries the title/description, so later phases can flesh out the demo,
// sections and per-port text without changing the plumbing.
// ---------------------------------------------------------------------------

// --- Pure Data helpers -----------------------------------------------------

// The selector the Pd binding routes control messages by: the control name with
// every character outside [a-zA-Z0-9.~] replaced by '_' (mirrors
// avnd::fixup_identifier with pd::valid_char_for_name). A message
// [<selector> <value>( sent to the object's left inlet sets that control.
std::string selector(std::string_view name)
{
  std::string s;
  s.reserve(name.size());
  for(char c : name)
  {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                    || (c >= '0' && c <= '9') || c == '.' || c == '~';
    s += ok ? c : '_';
  }
  return s;
}

// Escape Pd's structural characters in free text (comments / message contents).
std::string pd_escape(std::string_view s)
{
  std::string r;
  r.reserve(s.size());
  for(char c : s)
  {
    if(c == ',' || c == ';' || c == '\\')
      r += '\\';
    r += c;
  }
  return r;
}

std::string trim_num(double v)
{
  std::string s = std::to_string(v);
  if(s.find('.') != std::string::npos)
  {
    while(!s.empty() && s.back() == '0')
      s.pop_back();
    if(!s.empty() && s.back() == '.')
      s.pop_back();
  }
  return s;
}

// Accumulates objects (each gets an index in creation order) and connections,
// which Pd references by that index. Objects are written first, connections
// after.
struct pd_patch
{
  std::vector<std::string> objects;
  std::vector<std::string> connections;

  int add(std::string line)
  {
    objects.push_back(std::move(line));
    return static_cast<int>(objects.size()) - 1;
  }
  void connect(int src, int outlet, int dst, int inlet)
  {
    connections.push_back(
        "#X connect " + std::to_string(src) + " " + std::to_string(outlet) + " "
        + std::to_string(dst) + " " + std::to_string(inlet) + ";");
  }
  // Emit a comment with an explicit character width (the `, f <n>` flag) so Pd's
  // line-wrapping is deterministic, and return the vertical space it occupies in
  // pixels. Callers advance Y by this so a comment that wraps to several lines
  // never collides with the next one.
  int text(int x, int y, std::string_view t, int width_chars = 60)
  {
    add("#X text " + std::to_string(x) + " " + std::to_string(y) + " "
        + pd_escape(t) + ", f " + std::to_string(width_chars) + ";");
    const int len = static_cast<int>(t.size());
    const int lines = len <= width_chars ? 1 : (len + width_chars - 1) / width_chars;
    return lines * 15; // ~one line height at font size 12
  }
  // A section-header divider bar, in the ELSE help-patch idiom.
  void divider(int x, int y, std::string_view label)
  {
    add("#X obj " + std::to_string(x) + " " + std::to_string(y)
        + " cnv 3 550 3 empty empty " + pd_escape(label)
        + " 8 12 0 13 #dcdcdc #000000 0;");
  }
  void write(std::ostream& o, int w, int h) const
  {
    o << "#N canvas 50 50 " << w << " " << h << " 12;\n";
    for(const auto& l : objects)
      o << l << '\n';
    for(const auto& c : connections)
      o << c << '\n';
  }
};

// Emit the widget that drives one control. Returns {widget_index, message_index}
// where the message [selector ...( is wired to the object's left inlet. Returns
// widget_index = -1 when only a message box (no live widget) is produced.
struct pd_driver { int widget = -1; int message = -1; };

pd_driver pd_emit_control(pd_patch& p, const port_t& c, int x, int y)
{
  const std::string sel = selector(c.name);
  pd_driver d;

  auto msg = [&](const std::string& body) {
    d.message = p.add(
        "#X msg " + std::to_string(x + 150) + " " + std::to_string(y) + " " + body
        + ";");
  };

  if(c.value_type == "bool" || c.widget == "toggle" || c.widget == "checkbox")
  {
    d.widget = p.add(
        "#X obj " + std::to_string(x) + " " + std::to_string(y)
        + " tgl 17 0 empty empty empty 17 7 0 10 #fcfcfc #000000 #000000 0 1;");
    msg(sel + " \\$1");
  }
  else if(c.value_type == "enum" && !c.choices.empty())
  {
    const int n = static_cast<int>(c.choices.size());
    d.widget = p.add(
        "#X obj " + std::to_string(x) + " " + std::to_string(y) + " hradio 17 1 "
        + std::to_string(n)
        + " 0 empty empty empty 0 -8 0 10 #fcfcfc #000000 #000000 0;");
    msg(sel + " \\$1"); // enum settable by index
  }
  else if(c.value_type == "string")
  {
    d.widget = p.add(
        "#X obj " + std::to_string(x) + " " + std::to_string(y)
        + " symbolatom 12 0 0 0 - - - 0;");
    msg(sel + " \\$1");
  }
  else if(c.value_type == "int")
  {
    d.widget
        = p.add("#X floatatom " + std::to_string(x) + " " + std::to_string(y)
                + " 5 0 0 0 - - - 0;");
    msg(sel + " \\$1");
  }
  else if(c.value_type == "float")
  {
    if(c.range && c.range->min && c.range->max)
    {
      d.widget = p.add(
          "#X obj " + std::to_string(x) + " " + std::to_string(y) + " hsl 128 15 "
          + trim_num(*c.range->min) + " " + trim_num(*c.range->max)
          + " 0 0 empty empty empty -2 -8 0 10 #fcfcfc #000000 #000000 0 1;");
    }
    else
    {
      d.widget
          = p.add("#X floatatom " + std::to_string(x) + " " + std::to_string(y)
                  + " 5 0 0 0 - - - 0;");
    }
    msg(sel + " \\$1");
  }
  else
  {
    // bang / impulse, or a complex/multi-component control: emit an editable
    // message box the user can click, prefilled with the selector.
    msg(sel);
  }

  if(d.widget >= 0 && d.message >= 0)
    p.connect(d.widget, 0, d.message, 0);
  return d;
}

std::string pd_port_typestr(const port_t& c)
{
  if(c.type == "parameter")
  {
    std::string s = c.value_type.empty() ? "control" : c.value_type;
    if(c.range && c.range->min && c.range->max)
      s += " (" + trim_num(*c.range->min) + ".." + trim_num(*c.range->max) + ")";
    return s;
  }
  if(c.type == "audio")
    return "signal";
  return c.type;
}

// Pure Data netlist help patch: title, description, a live interactive demo
// (a widget wired to each control's left-inlet message, osc~ sources for audio
// in, clip~/dac~ for audio out, sinks on outlets), and labelled
// inlets/outlets/arguments sections.
void emit_pd(const model_t& m, std::ostream& out)
{
  pd_patch p;

  // --- header: title + library badge ---
  p.add("#X obj 4 5 cnv 15 360 42 empty empty " + pd_escape(m.name)
        + " 20 20 2 37 #e0e0e0 #000000 0;");
  p.add("#X obj 470 5 cnv 15 130 42 empty empty avendish 12 13 0 18 #7c7c7c "
        "#e0e4dc 0;");

  // --- description ---
  p.text(8, 54, blurb(m), 86);

  // --- the object (created early so its index is known to connections) ---
  std::vector<const port_t*> controls, audio_in;
  for(const auto& c : m.inputs)
  {
    if(c.type == "parameter")
      controls.push_back(&c);
    else if(c.type == "audio")
      audio_in.push_back(&c);
  }

  const int demo_top = 92;
  const int row = 30;
  const int rows_per_col = 18; // wrap long control lists into columns
  const int col_w = 330;
  const int nctrl = static_cast<int>(controls.size());
  const int tallest = nctrl > 0 ? std::min(nctrl, rows_per_col) : 0;
  const int msgs_h = static_cast<int>(m.messages.size()) * row;
  const int y_obj = demo_top + tallest * row + msgs_h + 50;

  const std::string create = m.c_name.empty() ? m.name : m.c_name;
  const int obj_idx = p.add(
      "#X obj 24 " + std::to_string(y_obj) + " " + pd_escape(create) + ";");

  // --- controls: one driver per control, wired to the left inlet; per-control
  // descriptions live in the inlets section below to keep the demo compact ---
  for(int i = 0; i < nctrl; ++i)
  {
    const int cx = 24 + (i / rows_per_col) * col_w;
    const int cy = demo_top + (i % rows_per_col) * row;
    pd_driver d = pd_emit_control(p, *controls[i], cx, cy);
    if(d.message >= 0)
      p.connect(d.message, 0, obj_idx, 0);
  }

  // --- messages: clickable message boxes into the left inlet ---
  int y = demo_top + tallest * row + 10;
  for(const auto& msg : m.messages)
  {
    const int mi = p.add(
        "#X msg 24 " + std::to_string(y) + " " + selector(msg.name) + ";");
    p.connect(mi, 0, obj_idx, 0);
    y += row;
  }

  // --- audio inputs: osc~ sources into the signal inlets ---
  int ax = 24;
  for(std::size_t k = 0; k < audio_in.size(); ++k)
  {
    const int osc = p.add(
        "#X obj " + std::to_string(ax) + " " + std::to_string(y_obj - 40)
        + " osc~ 220;");
    p.connect(osc, 0, obj_idx, static_cast<int>(k));
    ax += 70;
  }

  // --- outputs: sinks in dump order (= outlet order) ---
  int dac_idx = -1;
  int ox = 24, oy = y_obj + 44;
  for(std::size_t i = 0; i < m.outputs.size(); ++i)
  {
    const port_t& o = m.outputs[i];
    if(o.type == "audio")
    {
      const int clip = p.add(
          "#X obj " + std::to_string(ox) + " " + std::to_string(oy)
          + " clip~ -0.95 0.95;");
      p.connect(obj_idx, static_cast<int>(i), clip, 0);
      if(dac_idx < 0)
        dac_idx = p.add("#X obj 24 " + std::to_string(oy + 40) + " dac~;");
      p.connect(clip, 0, dac_idx, static_cast<int>(i) % 2);
      ox += 70;
    }
    else if(o.type == "message" || o.type == "callback")
    {
      const int pr = p.add(
          "#X obj " + std::to_string(ox) + " " + std::to_string(oy) + " print "
          + selector(o.name) + ";");
      p.connect(obj_idx, static_cast<int>(i), pr, 0);
      ox += 90;
    }
    else
    {
      const int fa = p.add(
          "#X floatatom " + std::to_string(ox) + " " + std::to_string(oy)
          + " 5 0 0 0 - - - 0;");
      p.connect(obj_idx, static_cast<int>(i), fa, 0);
      ox += 60;
    }
  }

  // --- reference sections (text only) below the demo ---
  int sy = (dac_idx >= 0 ? y_obj + 44 + 80 : oy + 40);
  if(sy < y_obj + 90)
    sy = y_obj + 90;
  p.divider(8, sy, "inlets");
  sy += 20;
  sy += p.text(
      20, sy, "left inlet: any [name value( message sets the matching control");
  for(const auto* c : controls)
    sy += p.text(20, sy, selector(c->name) + " - " + pd_port_typestr(*c)
                             + (c->description.empty() ? "" : ": " + c->description));
  for(const auto& msg : m.messages)
    sy += p.text(20, sy, selector(msg.name) + " - message");

  if(!m.outputs.empty())
  {
    sy += 10;
    p.divider(8, sy, "outlets");
    sy += 20;
    int oi = 0;
    for(const auto& o : m.outputs)
      sy += p.text(20, sy, std::to_string(oi++) + ") " + o.name + " - "
                               + pd_port_typestr(o)
                               + (o.description.empty() ? "" : ": " + o.description));
  }

  const int ncols = nctrl > 0 ? (nctrl + rows_per_col - 1) / rows_per_col : 1;
  const int width = std::max(620, 24 + ncols * col_w + 80);
  const int height = sy + 40;
  p.write(out, width, height < 300 ? 300 : height);
}

// Max/MSP .maxhelp is a JSON patcher (same schema family as .maxpat). Controls
// are driven exactly like Pd: a [selector value( message into the object's left
// inlet (the binding registers an "anything" A_GIMME method), with the same
// name normalization.
struct max_patch
{
  json boxes = json::array();
  json lines = json::array();
  int n = 0;

  std::string box(
      std::string_view maxclass, std::string_view text, double x, double y,
      double w, double h, int nin = 1, int nout = 1)
  {
    std::string id = "obj-" + std::to_string(++n);
    json b;
    b["id"] = id;
    b["maxclass"] = maxclass;
    b["numinlets"] = nin;
    b["numoutlets"] = nout;
    b["patching_rect"] = {x, y, w, h};
    if(!text.empty())
      b["text"] = text;
    if(nout > 0)
      b["outlettype"] = std::vector<std::string>(static_cast<std::size_t>(nout), "");
    boxes.push_back(json{{"box", b}});
    return id;
  }
  void line(const std::string& s, int so, const std::string& d, int di)
  {
    lines.push_back(json{
        {"patchline",
         {{"source", {s, so}}, {"destination", {d, di}}}}});
  }
  void write(std::ostream& o) const
  {
    json p;
    p["patcher"] = {
        {"fileversion", 1},
        {"appversion",
         {{"major", 8}, {"minor", 5}, {"revision", 0}, {"architecture", "x64"},
          {"modernui", 1}}},
        {"classnamespace", "box"},
        {"rect", {80.0, 80.0, 900.0, 600.0}},
        {"boxes", boxes},
        {"lines", lines}};
    json doc;
    doc["patcher"] = p["patcher"];
    o << doc.dump(2) << '\n';
  }
};

void emit_max(const model_t& m, std::ostream& out)
{
  max_patch p;

  p.box("comment", m.name, 40, 16, 400, 20, 1, 0);
  p.box("comment", blurb(m), 40, 38, 600, 20, 1, 0);

  std::vector<const port_t*> controls, audio_in;
  for(const auto& c : m.inputs)
  {
    if(c.type == "parameter")
      controls.push_back(&c);
    else if(c.type == "audio")
      audio_in.push_back(&c);
  }

  const double row = 36;
  const double demo_top = 90;
  const double y_obj
      = demo_top + (controls.size() + m.messages.size()) * row + 60;

  const std::string create = m.c_name.empty() ? m.name : m.c_name;
  const std::string obj = p.box(
      "newobj", create, 40, y_obj, 240, 22,
      std::max<int>(1, static_cast<int>(audio_in.size())),
      std::max<int>(1, static_cast<int>(m.outputs.size())));

  double y = demo_top;
  for(const auto* c : controls)
  {
    const std::string sel = selector(c->name);
    std::string widget, body = sel + " $1";
    std::string maxclass;
    if(c->value_type == "bool")
      maxclass = "toggle";
    else if(c->value_type == "int" || c->value_type == "enum")
      maxclass = "number";
    else if(c->value_type == "float")
      maxclass = "flonum";
    else if(c->value_type == "string")
      maxclass = ""; // message-only
    else
    {
      maxclass = "button";
      body = sel; // bang / impulse
    }

    if(!maxclass.empty())
    {
      widget = p.box(maxclass, "", 40, y, 50, 22);
      const std::string msg = p.box("message", body, 200, y, 140, 22);
      p.line(widget, 0, msg, 0);
      p.line(msg, 0, obj, 0);
    }
    else
    {
      const std::string msg = p.box("message", sel + " value", 200, y, 140, 22);
      p.line(msg, 0, obj, 0);
    }
    p.box("comment", c->name + " - " + pd_port_typestr(*c)
                         + (c->description.empty() ? "" : ": " + c->description),
          350, y, 240, 20, 1, 0);
    y += row;
  }

  for(const auto& msg : m.messages)
  {
    const std::string mb = p.box("message", selector(msg.name), 40, y, 140, 22);
    p.line(mb, 0, obj, 0);
    y += row;
  }

  double ax = 40;
  for(std::size_t k = 0; k < audio_in.size(); ++k)
  {
    const std::string src = p.box("newobj", "cycle~ 220", ax, y_obj - 40, 90, 22);
    p.line(src, 0, obj, static_cast<int>(k));
    ax += 100;
  }

  double ox = 40;
  const double oy = y_obj + 50;
  std::string dac;
  for(std::size_t i = 0; i < m.outputs.size(); ++i)
  {
    const port_t& o = m.outputs[i];
    if(o.type == "audio")
    {
      const std::string g = p.box("newobj", "*~ 0.2", ox, oy, 60, 22);
      p.line(obj, static_cast<int>(i), g, 0);
      if(dac.empty())
        dac = p.box("newobj", "ezdac~", 40, oy + 40, 45, 45, 2, 0);
      p.line(g, 0, dac, static_cast<int>(i) % 2);
      ox += 80;
    }
    else
    {
      const std::string pr
          = p.box("newobj", "print " + selector(o.name), ox, oy, 140, 22, 1, 0);
      p.line(obj, static_cast<int>(i), pr, 0);
      ox += 150;
    }
  }

  p.write(out);
}

// Godot text scene (.tscn) — fully text-emittable. Instantiates the
// extension-registered class and sets a few exported properties to their init
// values. `cls` is the registered class name (avnd_<c_name><suffix>); when
// absent we fall back to avnd_<c_name>.
bool valid_gd_ident(std::string_view s)
{
  if(s.empty())
    return false;
  if(!((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z') || s[0] == '_'))
    return false;
  for(char c : s)
    if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
         || c == '_'))
      return false;
  return true;
}

std::string gd_node_name(std::string_view s)
{
  std::string r;
  for(char c : s)
    r += (c == '.' || c == ':' || c == '@' || c == '/' || c == '%' || c == ' ')
             ? '_'
             : c;
  return r.empty() ? std::string{"Example"} : r;
}

void emit_godot(const model_t& m, std::ostream& out, const std::string& cls)
{
  const std::string klass
      = !cls.empty() ? cls
                     : "avnd_" + (m.c_name.empty() ? m.name : m.c_name);

  out << "[gd_scene format=3]\n\n";
  out << "; " << m.name << " - " << blurb(m) << " (auto-generated example)\n\n";
  out << "[node name=\"" << gd_node_name(m.name) << "\" type=\"" << klass << "\"]\n";

  // Exported properties (Avendish parameters). Property names are the control
  // names verbatim; emit assignments for the ones that are valid identifiers.
  for(const auto& c : m.inputs)
  {
    if(c.type != "parameter" || !valid_gd_ident(c.name))
      continue;
    if(c.value_type == "float" || c.value_type == "int")
    {
      double v = 0.0;
      if(c.range && c.range->init)
        v = *c.range->init;
      else if(c.default_value && c.default_value->is_number())
        v = c.default_value->get<double>();
      out << c.name << " = " << trim_num(v) << "\n";
    }
    else if(c.value_type == "bool")
    {
      bool v = c.default_value && c.default_value->is_boolean()
               && c.default_value->get<bool>();
      out << c.name << " = " << (v ? "true" : "false") << "\n";
    }
    else if(c.value_type == "enum")
    {
      out << c.name << " = 0\n";
    }
  }
}

// TouchDesigner example: a Python network-builder script. An avendish TD
// operator is a compiled Custom OP plugin, so a network can only reference it
// once the plugin is installed -- the robust shippable artifact is a builder
// script (run inside TD) or an author-provided .tox via the EXAMPLE_TD override.
// `optype` is the registered Custom OP type (passed by CMake); falls back to the
// c_name.
void emit_td(const model_t& m, std::ostream& out, const std::string& optype)
{
  const std::string ty = optype.empty() ? m.c_name : optype;
  out << "# TouchDesigner example builder for " << m.name << "\n";
  out << "# " << blurb(m) << "\n";
  out << "#\n";
  out << "# Run inside TouchDesigner (paste into a Text DAT and run it, or call\n";
  out << "# build(op('/')) ) to create an example network for this operator.\n";
  out << "# A hand-authored .tox can be shipped instead via the EXAMPLE_TD override.\n\n";
  out << "OPERATOR_TYPE = " << '"' << ty << '"' << "\n\n";
  out << "# Parameters (name, type, range/default):\n";
  for(const auto& c : m.inputs)
  {
    if(c.type != "parameter")
      continue;
    out << "#   - " << c.name << " : " << pd_port_typestr(c);
    if(!c.description.empty())
      out << " - " << c.description;
    out << "\n";
  }
  out << "\n";
  out << "def build(parent):\n";
  out << "    n = parent.create(OPERATOR_TYPE, " << '"' << m.c_name << "_example"
      << '"' << ")\n";
  for(const auto& c : m.inputs)
  {
    if(c.type != "parameter")
      continue;
    double v = 0.0;
    if(c.range && c.range->init)
      v = *c.range->init;
    else if(c.default_value && c.default_value->is_number())
      v = c.default_value->get<double>();
    // Custom parameters appear under n.par.<Name> (TD capitalizes the first
    // letter of the tuplet name); leave the assignment for the user to confirm.
    out << "    # n.par." << c.name << " = " << trim_num(v) << "\n";
  }
  out << "    return n\n";
}

// Python example script: import the module and exercise the object.
void emit_python(const model_t& m, std::ostream& out)
{
  out << "#!/usr/bin/env python3\n";
  out << "\"\"\"" << m.name << " - " << blurb(m) << "\n\n";
  out << "Auto-generated usage example.\n\"\"\"\n\n";
  out << "import " << m.c_name << " as mod\n\n";
  out << "obj = mod." << m.c_name << "()\n\n";
  out << "# Set the input controls:\n";
  bool any = false;
  for(const auto& c : m.inputs)
  {
    if(c.type != "parameter")
      continue;
    any = true;
    std::string v = "0";
    if(c.value_type == "string")
      v = "\"\"";
    else if(c.value_type == "bool")
      v = "False";
    else if(c.range && c.range->init)
      v = trim_num(*c.range->init);
    else if(c.default_value && c.default_value->is_number())
      v = trim_num(c.default_value->get<double>());
    out << "obj." << selector(c.name) << " = " << v;
    if(!c.description.empty())
      out << "  # " << c.description;
    out << "\n";
  }
  if(!any)
    out << "# (this object has no input controls)\n";
}

int run(const std::string& backend, const std::string& in_path,
        const std::string& out_path, const std::string& hint)
{
  std::ifstream in(in_path);
  if(!in)
  {
    std::cerr << "generate_patches: cannot open input '" << in_path << "'\n";
    return 2;
  }

  json j;
  try
  {
    in >> j;
  }
  catch(const std::exception& e)
  {
    std::cerr << "generate_patches: invalid JSON in '" << in_path << "': " << e.what()
              << '\n';
    return 3;
  }

  const model_t m = parse_model(j);

  if(const auto parent = std::filesystem::path(out_path).parent_path();
     !parent.empty())
  {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
  }

  std::ofstream out(out_path, std::ios::binary);
  if(!out)
  {
    std::cerr << "generate_patches: cannot open output '" << out_path << "'\n";
    return 4;
  }

  if(backend == "pd")
    emit_pd(m, out);
  else if(backend == "max" || backend == "maxhelp")
    emit_max(m, out);
  else if(backend == "godot")
    emit_godot(m, out, hint);
  else if(backend == "td" || backend == "touchdesigner")
    emit_td(m, out, hint);
  else if(backend == "python")
    emit_python(m, out);
  else
  {
    std::cerr << "generate_patches: unknown backend '" << backend << "'\n";
    return 5;
  }
  return 0;
}
}

int main(int argc, char** argv)
{
  if(argc != 4 && argc != 5)
  {
    std::cerr
        << "Usage: generate_patches <backend> <input.json> <output-path> [hint]\n"
           "  backend: pd | max | godot | td | python\n"
           "  hint:    backend-specific (godot: the registered class name)\n";
    return 1;
  }
  return run(argv[1], argv[2], argv[3], argc == 5 ? argv[4] : std::string{});
}
