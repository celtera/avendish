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

// Pure Data netlist (#N canvas / #X obj / #X text / #X connect ...).
void emit_pd(const model_t& m, std::ostream& out)
{
  out << "#N canvas 50 50 620 440 12;\n";
  out << "#X text 18 16 " << m.name << " \\, " << blurb(m)
      << " (auto-generated help -- WIP);\n";
  // Instantiate the object so the patch is already meaningful.
  out << "#X obj 18 80 " << m.c_name << ";\n";
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
