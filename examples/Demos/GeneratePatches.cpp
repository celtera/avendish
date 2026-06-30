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
std::string pd_selector(std::string_view name)
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
  void text(int x, int y, std::string_view t)
  {
    add("#X text " + std::to_string(x) + " " + std::to_string(y) + " "
        + pd_escape(t) + ";");
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
  const std::string sel = pd_selector(c.name);
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
  p.text(8, 54, blurb(m));

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
  const int controls_h = static_cast<int>(controls.size()) * row;
  const int msgs_h = static_cast<int>(m.messages.size()) * row;
  const int y_obj = demo_top + controls_h + msgs_h + 50;

  const std::string create = m.c_name.empty() ? m.name : m.c_name;
  const int obj_idx = p.add(
      "#X obj 24 " + std::to_string(y_obj) + " " + pd_escape(create) + ";");

  // --- controls: one driver per control, wired to the left inlet ---
  int y = demo_top;
  for(const auto* c : controls)
  {
    pd_driver d = pd_emit_control(p, *c, 24, y);
    if(d.message >= 0)
      p.connect(d.message, 0, obj_idx, 0);
    p.text(320, y, c->name + " - " + pd_port_typestr(*c)
                       + (c->description.empty() ? "" : ": " + c->description));
    y += row;
  }

  // --- messages: clickable message boxes into the left inlet ---
  for(const auto& msg : m.messages)
  {
    std::string body = pd_selector(msg.name);
    const int mi = p.add(
        "#X msg 24 " + std::to_string(y) + " " + body + ";");
    p.connect(mi, 0, obj_idx, 0);
    p.text(320, y, "message: " + msg.name);
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
          + pd_selector(o.name) + ";");
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
  sy += 18;
  p.text(20, sy, "left inlet: any [name value( message sets the matching control");
  sy += 18;
  for(const auto* c : controls)
  {
    p.text(20, sy, pd_selector(c->name) + " - " + pd_port_typestr(*c)
                       + (c->description.empty() ? "" : ": " + c->description));
    sy += 16;
  }
  for(const auto& msg : m.messages)
  {
    p.text(20, sy, pd_selector(msg.name) + " - message");
    sy += 16;
  }

  if(!m.outputs.empty())
  {
    sy += 8;
    p.divider(8, sy, "outlets");
    sy += 18;
    int oi = 0;
    for(const auto& o : m.outputs)
    {
      p.text(20, sy, std::to_string(oi++) + ") " + o.name + " - "
                         + pd_port_typestr(o)
                         + (o.description.empty() ? "" : ": " + o.description));
      sy += 16;
    }
  }

  const int height = sy + 40;
  p.write(out, 620, height < 300 ? 300 : height);
}

// Max/MSP .maxhelp is a JSON patcher (same schema family as .maxpat).
void emit_max(const model_t& m, std::ostream& out)
{
  json patcher;
  patcher["fileversion"] = 1;
  patcher["appversion"]
      = {{"major", 8}, {"minor", 0}, {"revision", 0}, {"architecture", "x64"},
         {"modernui", 1}};
  patcher["rect"] = {50, 50, 620, 440};
  patcher["boxes"] = json::array();

  auto comment = json::object();
  comment["box"]
      = {{"id", "obj-1"},
         {"maxclass", "comment"},
         {"text", m.name + " - " + blurb(m) + " (auto-generated help - WIP)"},
         {"patching_rect", {18.0, 16.0, 400.0, 20.0}}};
  patcher["boxes"].push_back(comment);

  auto object = json::object();
  object["box"]
      = {{"id", "obj-2"}, {"maxclass", "newobj"}, {"text", m.c_name},
         {"numinlets", 1}, {"numoutlets", 0},
         {"patching_rect", {18.0, 80.0, 120.0, 22.0}}};
  patcher["boxes"].push_back(object);

  patcher["lines"] = json::array();

  json doc;
  doc["patcher"] = patcher;
  out << doc.dump(2) << '\n';
}

// Godot text scene (.tscn). Phase 0: a Node placeholder + the description; the
// real generated/extension class is referenced in a later phase.
void emit_godot(const model_t& m, std::ostream& out)
{
  out << "[gd_scene format=3]\n\n";
  out << "; " << m.name << " - " << blurb(m) << " (auto-generated example - WIP)\n\n";
  out << "[node name=\"" << m.name << "Example\" type=\"Node\"]\n";
}

// TouchDesigner: the real path is text-synthesis + toecollapse (Phase 3). Phase
// 0 emits a placeholder describing the object's parameters as plain text.
void emit_td(const model_t& m, std::ostream& out)
{
  out << "# TouchDesigner example for " << m.name << " (" << m.c_name << ")\n";
  out << "# " << blurb(m) << "\n";
  out << "# Auto-generated placeholder -- real .tox synthesis lands in Phase 3.\n";
  out << "# Parameters:\n";
  for(const auto& p : m.inputs)
    out << "#   - " << p.name << " (" << (p.value_type.empty() ? p.type : p.value_type)
        << ")\n";
}

// Python example script.
void emit_python(const model_t& m, std::ostream& out)
{
  out << "# Auto-generated example for " << m.name << " (WIP)\n";
  out << "# " << blurb(m) << "\n";
  out << "import " << m.c_name << " as obj\n";
}

int run(const std::string& backend, const std::string& in_path,
        const std::string& out_path)
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
    emit_godot(m, out);
  else if(backend == "td" || backend == "touchdesigner")
    emit_td(m, out);
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
  if(argc != 4)
  {
    std::cerr
        << "Usage: generate_patches <backend> <input.json> <output-path>\n"
           "  backend: pd | max | godot | td | python\n";
    return 1;
  }
  return run(argv[1], argv[2], argv[3]);
}
