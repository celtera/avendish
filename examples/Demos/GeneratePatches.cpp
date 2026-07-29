/* SPDX-License-Identifier: GPL-3.0-or-later */

// Generate help / example patches for an avendish object, for one backend, from
// the object's introspection dump (dump/<c_name>.json produced by the dump
// backend). It is invoked per object at build time, mirroring json_to_maxref.
//
//   generate_patches <backend> <input.json> <output-path> [hint]
//
// backend ∈ { pd, max, godot, td, python }
//
// Design notes (see HELP_PATCH_QUALITY_PLAN.md):
//
//  * Every emitted box is *measured* with the host's real font metrics and
//    placed by a flow layout, so no two boxes can overlap and the canvas is
//    sized from the content. Pd's metrics come from its own font table
//    (tcl/pd-gui.tcl); Max needs an explicit rect + linecount per comment
//    because it never auto-sizes one.
//  * Connections follow the topology the bindings actually build (one inlet per
//    port for message objects, a single multichannel signal inlet for Max audio
//    objects, ...) rather than assuming port index == inlet index.
//  * Value outputs land in number / toggle / symbol boxes, because that is what
//    the bindings send; `print` is reserved for the callbacks and messages that
//    are genuinely selector-prefixed.

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

  // Ports declaring symbol()/c_name() have their outlet messages prefixed with
  // that selector (value_to_pd_dispatch / max::outputs), so a numeric sink
  // needs a [route <selector>] in front of it.
  std::string out_selector;

  // type == "parameter"
  std::string value_type; // float / int / bool / string / enum / list / array /
                          // aggregate / xy / rgba / ... / unknown
  bool is_control = false;
  bool is_value_port = false;
  bool is_attribute = false; // class_attribute: no inlet, set with [name value(
  std::optional<range_t> range;
  std::vector<std::string> choices; // enum / combobox
  std::optional<json> default_value;
  std::string widget;
  std::string unit;
  int components = 0; // fixed arity of array / aggregate value types

  // type == "audio"
  std::string audio_sample_format; // float / double
  std::string audio_port_format;   // sample / channel / bus / frame
  int audio_channels = 0;          // statically-known channel count, 0 = dynamic

  // type == "texture": "cpu" (a Jitter matrix in Max), "gpu", "gpu_binding".
  std::string storage;

  // A CPU texture / buffer is what makes an object a Jitter matrix operator.
  bool is_matrix() const
  {
    return (type == "texture" && storage == "cpu") || type == "tensor";
  }
};

// The number of Pd signal inlets/outlets a set of audio ports expands to:
// each statically-sized port contributes its channels; when only dynamic
// buses are present Pd defaults the whole bus set to a single channel.
int pd_signal_count(const std::vector<const port_t*>& audio_ports)
{
  if(audio_ports.empty())
    return 0;
  int fixed = 0;
  for(const auto* a : audio_ports)
    if(a->audio_channels > 0)
      fixed += a->audio_channels;
  return fixed > 0 ? fixed : 1;
}

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
  std::vector<std::string> init_arguments; // T::initialize signature
};

// Default creation arguments for an object requiring initialize(...) args, as
// a Pd/Max object-box suffix (numbers -> 0, strings -> a placeholder symbol).
std::string default_init_args(const model_t& m)
{
  std::string s;
  for(const auto& a : m.init_arguments)
  {
    if(a == "float" || a == "double" || a == "int" || a == "bool")
      s += " 0";
    else
      s += " arg";
  }
  return s;
}

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
  out.name = str(p, "name");
  out.description = str(p, "description");
  out.type = str(p, "type", "unknown");
  out.is_attribute = p.value("class_attribute", false);
  out.storage = str(p, "storage");
  // symbol() wins over c_name(): the bindings test has_symbol first.
  out.out_selector = str(p, "symbol");
  if(out.out_selector.empty())
    out.out_selector = str(p, "c_name");

  if(auto pit = p.find("parameter"); pit != p.end())
  {
    const json& par = *pit;
    out.value_type = str(par, "value_type", "unknown");
    out.is_control = par.value("control", false);
    out.is_value_port = par.value("value_port", false);
    out.widget = str(par, "widget");
    out.unit = str(par, "unit");
    if(auto rit = par.find("range"); rit != par.end())
      out.range = parse_range(*rit);
    if(auto cit = par.find("choices"); cit != par.end() && cit->is_array())
      for(const auto& c : *cit)
        if(c.is_string())
          out.choices.push_back(c.get<std::string>());
    if(auto dit = par.find("default"); dit != par.end())
      out.default_value = *dit;
    if(auto cit = par.find("components"); cit != par.end() && cit->is_number())
      out.components = cit->get<int>();
  }
  if(auto ait = p.find("audio"); ait != p.end())
  {
    out.audio_sample_format = str(*ait, "sample_format");
    out.audio_port_format = str(*ait, "port_format");
    if(auto cit = ait->find("channels"); cit != ait->end() && cit->is_number())
      out.audio_channels = cit->get<int>();
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
    if(m.author.empty())
      m.author = str(md, "vendor");
    m.version = str(md, "version");
  }
  // Unnamed parameter ports get the same positional fallback name the
  // runtime bindings and the golden reference files use: p<index> within the
  // parameter ports (the pd binding accepts [p<i> value( for them).
  auto name_ports = [](std::vector<port_t>& ports) {
    int param_idx = 0;
    for(auto& p : ports)
    {
      if(p.type == "parameter")
      {
        if(p.name.empty())
          p.name = "p" + std::to_string(param_idx);
        param_idx++;
      }
      else if(p.name.empty())
        p.name = p.type;
    }
  };
  if(auto it = j.find("inputs"); it != j.end() && it->is_array())
    for(const auto& p : *it)
      m.inputs.push_back(parse_port(p));
  if(auto it = j.find("outputs"); it != j.end() && it->is_array())
    for(const auto& p : *it)
      m.outputs.push_back(parse_port(p));
  name_ports(m.inputs);
  name_ports(m.outputs);
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
  if(auto it = j.find("init"); it != j.end())
    if(auto ait = it->find("arguments"); ait != it->end() && ait->is_array())
      for(const auto& a : *ait)
        m.init_arguments.push_back(a.is_string() ? a.get<std::string>() : a.dump());
  return m;
}

// ---------------------------------------------------------------------------
// Binding topology: which inlet / outlet does a port actually get?
//
// This mirrors the bindings exactly and is the difference between a patch whose
// connections load and one where Pd/Max silently drop half of them.
// ---------------------------------------------------------------------------
struct topology
{
  bool is_audio = false; // the object has audio ports -> DSP binding
  // A CPU matrix / texture port turns the Max object into a Jitter matrix
  // operator (binding/max/prototype.cpp.in picks jitter_processor when
  // max_jit_*_introspection > 0), which Max gives its own matrix inlet 0 and
  // matrix outlet 0 through max_jit_mop_setup.
  bool is_max_jitter = false;
  int max_matrix_in = 0;
  int max_matrix_out = 0;

  int pd_signal_inlets = 0;
  int pd_signal_outlets = 0;

  // Per input-port index: the inlet a widget can connect straight into, or -1
  // when the port has none (attributes, and every control of an audio object)
  // and must be driven by a [name value( message on inlet 0.
  std::vector<int> pd_inlet;
  std::vector<int> max_inlet;

  // Per output-port index: the outlet index, or -1 when the port has none.
  std::vector<int> pd_outlet;
  std::vector<int> max_outlet;
};

topology compute_topology(const model_t& m)
{
  topology t;

  std::vector<const port_t*> audio_in, audio_out;
  for(const auto& p : m.inputs)
    if(p.type == "audio")
      audio_in.push_back(&p);
  for(const auto& p : m.outputs)
    if(p.type == "audio")
      audio_out.push_back(&p);
  t.is_audio = !audio_in.empty() || !audio_out.empty();
  t.pd_signal_inlets = pd_signal_count(audio_in);
  t.pd_signal_outlets = pd_signal_count(audio_out);

  for(const auto& p : m.inputs)
    if(p.is_matrix())
      t.max_matrix_in++;
  for(const auto& p : m.outputs)
    if(p.is_matrix())
      t.max_matrix_out++;
  // The audio branch is tested first in prototype.cpp.in, so an object doing
  // both is a DSP object, not a matrix operator.
  t.is_max_jitter = !t.is_audio && (t.max_matrix_in > 0 || t.max_matrix_out > 0);

  t.pd_inlet.assign(m.inputs.size(), -1);
  t.max_inlet.assign(m.inputs.size(), -1);
  t.pd_outlet.assign(m.outputs.size(), -1);
  t.max_outlet.assign(m.outputs.size(), -1);

  // --- Pd inlets ---
  // pd::inputs::init creates one inlet per input port, skipping the very first
  // port (Pd gives every object a left inlet) and every attribute. An audio
  // object never gets control inlets at all: pd/audio_processor.hpp creates
  // only signal inlets and routes every message through the left inlet.
  if(!t.is_audio)
  {
    int next = 1;
    for(std::size_t i = 0; i < m.inputs.size(); ++i)
    {
      if(i == 0)
      {
        // The first port is addressed through the default left inlet.
        t.pd_inlet[i] = m.inputs[i].is_attribute ? -1 : 0;
        continue;
      }
      if(m.inputs[i].is_attribute)
        continue;
      t.pd_inlet[i] = next++;
    }
  }

  // --- Pd outlets ---
  if(t.is_audio)
  {
    // Signal outlets first, then one message outlet per non-audio output in
    // declaration order (pd/audio_processor.hpp).
    int next = t.pd_signal_outlets;
    int sig = 0;
    for(std::size_t i = 0; i < m.outputs.size(); ++i)
    {
      if(m.outputs[i].type == "audio")
      {
        t.pd_outlet[i] = sig;
        sig += m.outputs[i].audio_channels > 0 ? m.outputs[i].audio_channels : 1;
      }
      else
      {
        t.pd_outlet[i] = next++;
      }
    }
  }
  else
  {
    // pd::outputs::init: one outlet per output port, in declaration order.
    for(std::size_t i = 0; i < m.outputs.size(); ++i)
      t.pd_outlet[i] = static_cast<int>(i);
  }

  // --- Max inlets ---
  // max/audio_processor.hpp: dsp_setup(&x_obj, 1) -- a single multichannel
  // signal inlet, controls only through messages on it.
  // max/inputs.hpp: one inlet per *explicit* (non-attribute) parameter, shifted
  // by one when the first parameter port is an attribute (inlet 0 then serves
  // the attributes). Non-parameter inputs get no inlet.
  // A matrix operator gets its inlets from max_jit_mop_setup() before the
  // parameter proxies are created, so the plain "inlet k == explicit parameter
  // k" arithmetic no longer holds. Controls are driven with a
  // "<name> <value>" message on inlet 0 instead, which the binding routes
  // through process_inputs() just the same.
  if(!t.is_audio && !t.is_max_jitter)
  {
    bool first_param_is_explicit = true;
    bool seen_param = false;
    for(const auto& p : m.inputs)
    {
      if(p.type != "parameter")
        continue;
      first_param_is_explicit = !p.is_attribute;
      seen_param = true;
      break;
    }
    if(seen_param)
    {
      int j = 0;
      for(std::size_t i = 0; i < m.inputs.size(); ++i)
      {
        const auto& p = m.inputs[i];
        if(p.type != "parameter" || p.is_attribute)
          continue;
        t.max_inlet[i] = first_param_is_explicit ? j : j + 1;
        j++;
      }
    }
  }

  // --- Max outlets ---
  if(t.is_audio)
  {
    // The "multichannelsignal" outlet is created last and therefore ends up
    // leftmost (index 0); the control outlets sit to its right, in declaration
    // order. With no audio output at all they start at 0.
    const bool has_signal_out = !audio_out.empty();
    int next = has_signal_out ? 1 : 0;
    for(std::size_t i = 0; i < m.outputs.size(); ++i)
    {
      if(m.outputs[i].type == "audio")
        t.max_outlet[i] = 0;
      else
        t.max_outlet[i] = next++;
    }
  }
  else
  {
    for(std::size_t i = 0; i < m.outputs.size(); ++i)
      t.max_outlet[i] = static_cast<int>(i);
  }
  return t;
}

// ---------------------------------------------------------------------------
// Shared text helpers
// ---------------------------------------------------------------------------

// The selector the bindings route control messages by: the control name with
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

// Number of lines one paragraph occupies in a box `width` characters wide,
// using the same greedy break-at-the-last-space rule Pd's rtext and Max's
// comment renderer both apply. Words longer than the box are hard-broken.
int wrapped_lines_one(std::string_view text, int width)
{
  int lines = 0;
  std::size_t i = 0;
  const std::size_t n = text.size();
  if(n == 0)
    return 1;
  while(i < n)
  {
    if(n - i <= static_cast<std::size_t>(width))
    {
      lines++;
      break;
    }
    const std::size_t limit = i + static_cast<std::size_t>(width);
    std::size_t brk = text.rfind(' ', limit);
    if(brk == std::string_view::npos || brk <= i)
    {
      lines++;
      i = limit; // a single word wider than the box
    }
    else
    {
      lines++;
      i = brk + 1;
    }
  }
  return lines > 0 ? lines : 1;
}

// Total rendered line count, honouring the hard line breaks a description may
// carry (both hosts break on them, so they must be measured, not ignored).
int wrapped_lines(std::string_view text, int width)
{
  if(width <= 0)
    width = 60;
  int lines = 0;
  std::size_t start = 0;
  while(true)
  {
    const std::size_t nl = text.find('\n', start);
    lines += wrapped_lines_one(text.substr(start, nl - start), width);
    if(nl == std::string_view::npos)
      break;
    start = nl + 1;
  }
  return lines > 0 ? lines : 1;
}

// Pd stores a comment as a flat run of atoms: a raw newline is just whitespace
// there, so it must be folded away before measuring, or the box is sized for
// lines Pd will never draw.
std::string one_line(std::string_view s)
{
  std::string r;
  r.reserve(s.size());
  bool space = false;
  for(char c : s)
  {
    if(c == '\n' || c == '\r' || c == '\t' || c == ' ')
      space = true;
    else
    {
      if(space && !r.empty())
        r += ' ';
      space = false;
      r += c;
    }
  }
  return r;
}

// halp::impulse_button: the value is a std::optional<impulse_type>, so it is
// engaged by *any* message carrying its name -- a bare [<name>( -- and a "$1"
// message box would be a hard error when fed a bang.
bool is_impulse(const port_t& c)
{
  return c.widget == "bang" || c.widget == "impulse";
}

// halp::maintained_button: widget=button/pushbutton but the value is a plain
// bool, so it is driven exactly like a toggle.
bool is_bool_like(const port_t& c)
{
  return c.value_type == "bool" || c.widget == "toggle" || c.widget == "checkbox"
         || c.widget == "button" || c.widget == "pushbutton";
}

// Number of scalar components a multi-component control's demo message should
// carry: named shapes have a fixed arity, arrays/aggregates declare theirs in
// the dump, resizable lists get three demo elements. 0 = not multi-component.
int component_count(const port_t& c)
{
  if(c.value_type == "xy")
    return 2;
  if(c.value_type == "xyz" || c.value_type == "rgb")
    return 3;
  if(c.value_type == "xyzw" || c.value_type == "rgba")
    return 4;
  if(c.value_type == "array" || c.value_type == "aggregate")
    return c.components > 0 ? c.components : 3;
  if(c.value_type == "list")
    return 3;
  return 0;
}

// A sensible demo value for one component of a control: the range's init or
// midpoint when known, else 0.5.
std::string component_default(const port_t& c)
{
  if(c.range)
  {
    if(c.range->init)
      return trim_num(*c.range->init);
    if(c.range->min && c.range->max)
      return trim_num(*c.range->min + 0.5 * (*c.range->max - *c.range->min));
  }
  return "0.5";
}

std::string port_typestr(const port_t& c)
{
  if(c.type == "parameter")
  {
    std::string s = c.value_type.empty() ? "control" : c.value_type;
    if(is_impulse(c))
      s = "bang";
    else if(const int nc = component_count(c); nc > 0 && c.value_type != "list")
      s += " (" + std::to_string(nc) + " values)";
    else if(c.value_type == "list")
      s += " (any number of values)";
    if(c.range && c.range->min && c.range->max)
      s += " " + trim_num(*c.range->min) + ".." + trim_num(*c.range->max);
    if(!c.unit.empty())
      s += " " + c.unit;
    if(!c.choices.empty())
    {
      s += " [";
      for(std::size_t i = 0; i < c.choices.size(); ++i)
        s += (i ? ", " : "") + std::to_string(i) + "=" + c.choices[i];
      s += "]";
    }
    return s;
  }
  if(c.type == "audio")
    return "signal";
  return c.type;
}

// The one-line explanation shown next to a control in the demo.
std::string port_label(const port_t& c)
{
  std::string s = c.name + " - " + port_typestr(c);
  if(!c.description.empty())
    s += ": " + c.description;
  return s;
}

// The blurb under the title. Most objects declare no description, so synthesize
// something that still tells the user what they are looking at rather than
// printing "auto-generated help patch".
std::string blurb(const model_t& m, const topology& t)
{
  if(!m.short_description.empty())
    return m.short_description;
  if(!m.description.empty())
    return m.description;

  int controls = 0, others = 0;
  for(const auto& p : m.inputs)
  {
    if(p.type == "parameter")
      controls++;
    else if(p.type != "audio")
      others++;
  }
  std::string s = t.is_audio ? "audio object" : "control object";
  if(!m.category.empty())
    s = m.category + " " + s;
  s += " with " + std::to_string(controls)
       + (controls == 1 ? " control" : " controls");
  if(!m.outputs.empty())
    s += ", " + std::to_string(m.outputs.size())
         + (m.outputs.size() == 1 ? " output" : " outputs");
  if(!m.messages.empty())
    s += ", " + std::to_string(m.messages.size())
         + (m.messages.size() == 1 ? " message" : " messages");
  if(others > 0)
    s += ", " + std::to_string(others) + " non-audio data port(s)";
  s += ".";
  return s;
}

// ---------------------------------------------------------------------------
// Pure Data emitter
// ---------------------------------------------------------------------------

// Pd's fixed font sizes -> (char width, line height) in pixels, straight from
// `font_metrics` in Pd's tcl/pd-gui.tcl. We emit everything at size 12.
constexpr int PD_FW = 7;
constexpr int PD_FH = 16;
constexpr int PD_MAX_TEXT_COLS = 62; // comment width before wrapping

// Escape Pd's structural characters in free text (comments / message contents).
//
// A ',' or ';' must become an atom *of its own* -- "word \, next", the way Pd
// itself saves. Written glued to the previous word ("word\,") Pd displays the
// backslash; separated, it renders as plain "word, next", so the escaping costs
// nothing in the rendered width.
std::string pd_escape(std::string_view s)
{
  std::string r;
  r.reserve(s.size() + 8);
  for(char c : s)
  {
    if(c == ',' || c == ';')
    {
      if(!r.empty() && r.back() != ' ')
        r += ' ';
      r += '\\';
      r += c;
    }
    else if(c == '\\')
    {
      r += "\\\\";
    }
    else
    {
      r += c;
    }
  }
  return r;
}

// An iemgui label (cnv / hsl / tgl ...) is a *single atom*: an unescaped space
// ends it and every following argument shifts, which makes Pd reject the whole
// object and fall back to its default geometry. Escaped spaces are accepted and
// render as real spaces.
std::string pd_label(std::string_view s)
{
  if(s.empty())
    return "empty";
  std::string r;
  r.reserve(s.size() + 8);
  for(char c : s)
  {
    if(c == ' ' || c == ',' || c == ';' || c == '\\' || c == '$')
      r += '\\';
    r += c;
  }
  return r;
}

struct size_t2
{
  int w = 0, h = 0;
};

size_t2 pd_text_size(std::string_view t, int cols)
{
  const int lines = wrapped_lines(t, cols);
  const int used = lines == 1 ? static_cast<int>(std::min<std::size_t>(t.size(), cols))
                              : cols;
  return {used * PD_FW + 6, lines * PD_FH + 4};
}

size_t2 pd_box_size(std::string_view t)
{
  const int lines = wrapped_lines(t, PD_MAX_TEXT_COLS);
  const int used = lines == 1 ? static_cast<int>(t.size()) : PD_MAX_TEXT_COLS;
  return {used * PD_FW + 10, lines * PD_FH + 6};
}

// Accumulates objects (each gets an index in creation order) and connections,
// which Pd references by that index. Objects are written first, connections
// after. Every add_* returns the index *and* records the box's extent so the
// canvas can be sized from the content.
struct pd_patch
{
  std::vector<std::string> objects; // one entry = one Pd index (may be multi-line)
  std::vector<std::string> connections;
  int max_x = 0, max_y = 0;

  void extend(int x, int y, int w, int h)
  {
    max_x = std::max(max_x, x + w);
    max_y = std::max(max_y, y + h);
  }

  int add(std::string line, int x, int y, int w, int h)
  {
    objects.push_back(std::move(line));
    extend(x, y, w, h);
    return static_cast<int>(objects.size()) - 1;
  }

  void connect(int src, int outlet, int dst, int inlet)
  {
    if(src < 0 || dst < 0)
      return;
    connections.push_back(
        "#X connect " + std::to_string(src) + " " + std::to_string(outlet) + " "
        + std::to_string(dst) + " " + std::to_string(inlet) + ";");
  }

  int obj(int x, int y, const std::string& body)
  {
    const auto s = pd_box_size(body);
    return add(
        "#X obj " + std::to_string(x) + " " + std::to_string(y) + " " + body + ";",
        x, y, s.w, s.h);
  }
  int msg(int x, int y, const std::string& body)
  {
    const auto s = pd_box_size(body);
    return add(
        "#X msg " + std::to_string(x) + " " + std::to_string(y) + " " + body + ";",
        x, y, s.w, s.h);
  }
  int floatatom(int x, int y, int digits = 6)
  {
    return add(
        "#X floatatom " + std::to_string(x) + " " + std::to_string(y) + " "
            + std::to_string(digits) + " 0 0 0 - - - 0;",
        x, y, digits * PD_FW + 5, PD_FH + 4);
  }
  int symbolatom(int x, int y, int digits = 12)
  {
    return add(
        "#X symbolatom " + std::to_string(x) + " " + std::to_string(y) + " "
            + std::to_string(digits) + " 0 0 0 - - - 0;",
        x, y, digits * PD_FW + 5, PD_FH + 4);
  }

  // A comment. Returns the height it occupies so callers advance Y by the real
  // rendered height instead of guessing.
  int text(int x, int y, std::string_view t, int cols = PD_MAX_TEXT_COLS)
  {
    const std::string flat = one_line(t);
    const auto s = pd_text_size(flat, cols);
    add("#X text " + std::to_string(x) + " " + std::to_string(y) + " "
            + pd_escape(flat) + ", f " + std::to_string(cols) + ";",
        x, y, s.w, s.h);
    return s.h;
  }

  // A full-width horizontal rule, the vanilla-help separator.
  void rule(int x, int y, int w)
  {
    add("#X obj " + std::to_string(x) + " " + std::to_string(y) + " cnv 1 "
            + std::to_string(w) + " 1 empty empty empty 8 12 0 13 #909090 #909090 0;",
        x, y, w, 1);
  }

  // A section-header bar carrying a label, in the ELSE / vanilla idiom.
  void divider(int x, int y, int w, std::string_view label)
  {
    add("#X obj " + std::to_string(x) + " " + std::to_string(y) + " cnv 3 "
            + std::to_string(w) + " 3 empty empty " + pd_label(label)
            + " 8 12 0 13 #dcdcdc #000000 0;",
        x, y, w, 3);
  }

  // A subpatch occupying one index on the parent canvas. `body` is the already
  // rendered content of the child canvas.
  int subpatch(
      int x, int y, const std::string& name, int w, int h,
      const std::string& body)
  {
    std::string block = "#N canvas 80 80 " + std::to_string(w) + " "
                        + std::to_string(h) + " " + name + " 0;\n" + body
                        + "#X restore " + std::to_string(x) + " " + std::to_string(y)
                        + " pd " + name + ";";
    const auto s = pd_box_size("pd " + name);
    return add(std::move(block), x, y, s.w, s.h);
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

// The widget driving one control, plus how it reaches the object.
struct pd_driver
{
  int source = -1; // box whose outlet 0 carries the value
  int inlet = 0;   // object inlet it should be connected to
  int height = 0;  // vertical extent of everything emitted
  int width = 0;   // horizontal extent
};

// Emit the demo widget for one control at (x, y).
//
// `inlet` is the object inlet the port is reachable on, or -1 when it has none
// (attributes, and every control of an audio object) -- then the widget is
// routed through a [<selector> $1( message into inlet 0, which every binding
// accepts.
pd_driver pd_emit_control(pd_patch& p, const port_t& c, int x, int y, int inlet)
{
  const std::string sel = selector(c.name);
  pd_driver d;
  d.inlet = inlet >= 0 ? inlet : 0;

  int widget = -1;
  int w = 0, h = 0;

  if(is_impulse(c))
  {
    // A bare [<name>( engages the impulse; the message box is itself the
    // clickable widget, so no bng -> "$1" indirection (which would error).
    const int mi = p.msg(x, y, sel);
    const auto s = pd_box_size(sel);
    d.source = mi;
    d.inlet = 0;
    d.width = s.w;
    d.height = s.h;
    return d;
  }
  else if(is_bool_like(c))
  {
    widget = p.obj(
        x, y, "tgl 17 0 empty empty empty 17 7 0 10 #fcfcfc #000000 #000000 0 1");
    w = 18;
    h = 18;
  }
  else if(!c.choices.empty())
  {
    // Enums and comboboxes: a radio gives every option one click, and the
    // bindings accept the index as a float (from_atom on an enum rounds a
    // A_FLOAT and also resolves A_SYMBOL names).
    const int n = static_cast<int>(c.choices.size());
    widget = p.obj(
        x, y,
        "hradio 17 1 0 " + std::to_string(n)
            + " empty empty empty 0 -8 0 10 #fcfcfc #000000 #000000 0");
    w = 17 * n + 1;
    h = 18;
  }
  else if(c.value_type == "enum")
  {
    widget = p.floatatom(x, y, 5);
    w = 5 * PD_FW + 5;
    h = PD_FH + 4;
  }
  else if(c.value_type == "string")
  {
    widget = p.symbolatom(x, y, 12);
    w = 12 * PD_FW + 5;
    h = PD_FH + 4;
  }
  else if(c.value_type == "int")
  {
    widget = p.floatatom(x, y, 6);
    w = 6 * PD_FW + 5;
    h = PD_FH + 4;
  }
  else if(c.value_type == "float")
  {
    if(c.range && c.range->min && c.range->max)
    {
      widget = p.obj(
          x, y,
          "hsl 128 15 " + trim_num(*c.range->min) + " " + trim_num(*c.range->max)
              + " 0 0 empty empty empty -2 -8 0 10 #fcfcfc #000000 #000000 0 1");
      w = 131;
      h = 18;
    }
    else
    {
      widget = p.floatatom(x, y, 6);
      w = 6 * PD_FW + 5;
      h = PD_FH + 4;
    }
  }
  else if(const int nc = component_count(c); nc > 0)
  {
    // Multi-component control (xy / rgb / list ...): a clickable message
    // prefilled with one demo value per component.
    std::string body = sel;
    for(int i = 0; i < nc; i++)
      body += " " + component_default(c);
    widget = p.msg(x, y, body);
    const auto s = pd_box_size(body);
    w = s.w;
    h = s.h;
    d.source = widget;
    d.inlet = 0; // the message carries the selector, so it goes to the left inlet
    d.width = w;
    d.height = h;
    return d;
  }
  else
  {
    // A container/opaque control: an editable message box prefilled with the
    // selector, for the user to complete.
    widget = p.msg(x, y, sel);
    const auto s = pd_box_size(sel);
    d.source = widget;
    d.inlet = 0;
    d.width = s.w;
    d.height = s.h;
    return d;
  }

  if(inlet >= 0)
  {
    // The port has its own inlet: the widget's raw value goes straight in.
    d.source = widget;
    d.width = w;
    d.height = h;
  }
  else
  {
    // No inlet: wrap the value in a [<selector> $1( message on the left inlet.
    const std::string body = sel + " \\$1";
    const int mx = x + w + 12;
    const int mi = p.msg(mx, y, body);
    p.connect(widget, 0, mi, 0);
    const auto s = pd_box_size(body);
    d.source = mi;
    d.inlet = 0;
    d.width = w + 12 + s.w;
    d.height = std::max(h, s.h);
  }
  return d;
}

// The sink an output port's value should land in. Value ports emit bare
// floats/symbols/lists, so a number box shows them directly; only ports that
// declare symbol()/c_name() are selector-prefixed and need a [route] first.
// Callbacks carrying arguments stay on [print], which is what they are for.
struct pd_sink
{
  int box = -1;  // the box connected to the object's outlet
  int width = 0;
  int height = 0;
};

pd_sink pd_emit_sink(pd_patch& p, const port_t& o, int x, int y)
{
  pd_sink s;
  const std::string sel = selector(o.name);

  auto place_value_box = [&](int px, int py) -> std::pair<int, size_t2> {
    if(o.value_type == "bool")
      return {p.obj(px, py,
                    "tgl 17 0 empty empty empty 17 7 0 10 #fcfcfc #000000 "
                    "#000000 0 1"),
              size_t2{18, 18}};
    if(o.value_type == "string" || o.value_type == "enum")
      return {p.symbolatom(px, py, 12), size_t2{12 * PD_FW + 5, PD_FH + 4}};
    // A fixed-arity output sends a list: [unpack] it into one number box per
    // component, so the values are readable at a glance rather than scrolling
    // past in the console.
    if(const int nc = component_count(o); nc >= 2 && nc <= 6
                                          && o.value_type != "list")
    {
      std::string body = "unpack";
      for(int i = 0; i < nc; i++)
        body += " f";
      const int u = p.obj(px, py, body);
      const auto us = pd_box_size(body);
      const int fy = py + us.h + 8;
      constexpr int fw = 7 * PD_FW + 5;
      int fx = px;
      for(int i = 0; i < nc; i++)
      {
        const int fa = p.floatatom(fx, fy, 7);
        p.connect(u, i, fa, 0);
        fx += fw + 6;
      }
      return {u, size_t2{std::max(us.w, nc * (fw + 6) - 6), us.h + 8 + PD_FH + 4}};
    }
    if(component_count(o) > 0)
    {
      const std::string body = "print " + sel;
      return {p.obj(px, py, body), pd_box_size(body)};
    }
    return {p.floatatom(px, py, 7), size_t2{7 * PD_FW + 5, PD_FH + 4}};
  };

  if(o.type == "audio")
    return s; // handled by the audio chain

  const bool prefixed = !o.out_selector.empty();
  const bool message_like = o.type == "message" || o.type == "callback";

  if(message_like || o.type == "midi")
  {
    const std::string body = "print " + sel;
    s.box = p.obj(x, y, body);
    const auto sz = pd_box_size(body);
    s.width = sz.w;
    s.height = sz.h;
    return s;
  }

  if(prefixed)
  {
    // [route <selector>] strips the prefix the binding adds, then the value
    // reaches a real number box.
    const std::string body = "route " + selector(o.out_selector);
    const int r = p.obj(x, y, body);
    const auto rs = pd_box_size(body);
    auto [vb, vs] = place_value_box(x, y + rs.h + 8);
    p.connect(r, 0, vb, 0);
    s.box = r;
    s.width = std::max(rs.w, vs.w);
    s.height = rs.h + 8 + vs.h;
    return s;
  }

  auto [vb, vs] = place_value_box(x, y);
  s.box = vb;
  s.width = vs.w;
  s.height = vs.h;
  return s;
}

// The reference sections, rendered into the `pd reference` subpatch the way
// vanilla's own help patches do (doc/5.reference/*-help.pd). Keeping the dry
// documentation off the main canvas is what makes the demo readable.
std::string pd_reference_body(
    const model_t& m, const topology& t, const std::string& create, int& out_w,
    int& out_h)
{
  pd_patch r;
  const int col = 74; // the reference canvas is wider than the main one
  const int x = 12;
  int y = 10;

  y += r.text(x, y, m.name + " - " + blurb(m, t), col) + 6;

  auto section = [&](std::string_view title) {
    r.divider(x - 4, y, 550, title);
    y += 22;
  };

  section("INLETS");
  {
    const std::string intro
        = t.is_audio
              ? "Controls are set with a [<name> <value>( message on the left "
                "inlet; the signal inlets take audio."
              : "Each port below has its own inlet. Any [<name> <value>( "
                "message on the left inlet also works.";
    y += r.text(x + 8, y, intro, col);
    if(t.is_audio && t.pd_signal_inlets > 0)
      y += r.text(
          x + 8, y,
          "inlets 0.." + std::to_string(t.pd_signal_inlets - 1) + ": signal",
          col);
    for(std::size_t i = 0; i < m.inputs.size(); ++i)
    {
      const auto& c = m.inputs[i];
      if(c.type == "audio")
        continue;
      std::string line;
      if(t.pd_inlet[i] >= 0)
        line = "inlet " + std::to_string(t.pd_inlet[i]) + ": ";
      else if(c.is_attribute)
        line = "attribute (left inlet or creation arg): ";
      else
        line = "left inlet: ";
      line += port_label(c);
      y += r.text(x + 8, y, line, col);
    }
  }

  if(!m.messages.empty())
  {
    y += 8;
    section("MESSAGES");
    for(const auto& msg : m.messages)
    {
      std::string line = selector(msg.name);
      for(const auto& a : msg.arguments)
        line += " <" + a + ">";
      if(!msg.return_type.empty() && msg.return_type != "void")
        line += " -> " + msg.return_type;
      y += r.text(x + 8, y, line, col);
    }
  }

  if(!m.outputs.empty())
  {
    y += 8;
    section("OUTLETS");
    for(std::size_t i = 0; i < m.outputs.size(); ++i)
    {
      const auto& o = m.outputs[i];
      std::string line = "outlet " + std::to_string(t.pd_outlet[i]) + ": "
                         + port_label(o);
      if(!o.out_selector.empty())
        line += " (prefixed with '" + selector(o.out_selector) + "')";
      y += r.text(x + 8, y, line, col);
    }
  }

  if(!m.init_arguments.empty())
  {
    y += 8;
    section("ARGUMENTS");
    y += r.text(x + 8, y, "creation: [" + create + "]", col);
    for(std::size_t i = 0; i < m.init_arguments.size(); ++i)
      y += r.text(
          x + 8, y,
          std::to_string(i + 1) + ") " + m.init_arguments[i], col);
  }

  y += 8;
  section("ABOUT");
  if(!m.author.empty())
    y += r.text(x + 8, y, "author: " + m.author, col);
  if(!m.version.empty())
    y += r.text(x + 8, y, "version: " + m.version, col);
  if(!m.category.empty())
    y += r.text(x + 8, y, "category: " + m.category, col);
  y += r.text(x + 8, y, "generated by avendish from " + m.c_name, col);

  out_w = std::max(600, r.max_x + 30);
  out_h = std::max(300, r.max_y + 30);

  std::string body;
  for(const auto& l : r.objects)
    body += l + "\n";
  for(const auto& c : r.connections)
    body += c + "\n";
  return body;
}

void emit_pd(const model_t& m, std::ostream& out, const std::string& external_name)
{
  const topology t = compute_topology(m);
  pd_patch p;

  std::vector<std::size_t> controls, audio_in_idx, other_in;
  for(std::size_t i = 0; i < m.inputs.size(); ++i)
  {
    const auto& c = m.inputs[i];
    if(c.type == "parameter")
      controls.push_back(i);
    else if(c.type == "audio")
      audio_in_idx.push_back(i);
    else
      other_in.push_back(i);
  }

  const std::string create
      = (!external_name.empty() ? external_name
         : m.c_name.empty()     ? m.name
                                : m.c_name)
        + default_init_args(m);

  // --- header -------------------------------------------------------------
  const int left = 16;
  int y = 8;
  p.add("#X obj " + std::to_string(left) + " " + std::to_string(y)
            + " cnv 15 400 40 empty empty " + pd_label(m.name)
            + " 20 24 2 20 #e8e8e8 #000000 0;",
        left, y, 400, 40);
  p.add("#X obj " + std::to_string(left + 412) + " " + std::to_string(y)
            + " cnv 15 128 40 empty empty avendish 12 16 0 14 #7c7c7c #e0e4dc 0;",
        left + 412, y, 128, 40);
  y += 48;

  y += p.text(left, y, blurb(m, t), PD_MAX_TEXT_COLS + 12);
  y += 6;
  p.rule(left, y, 540);
  y += 12;

  // --- controls -----------------------------------------------------------
  // Every control gets a widget and a label beside it. Rows are as tall as the
  // tallest thing in them, so nothing can collide; long control lists wrap into
  // columns once a column reaches the row budget.
  constexpr int rows_per_col = 16;
  constexpr int row_gap = 8;
  constexpr int label_gap = 14;
  const int demo_top = y;

  int col_x = left;
  int col_y = demo_top;
  int col_widest = 0;
  int rows_in_col = 0;
  int demo_bottom = demo_top;

  struct wired
  {
    int source;
    int inlet;
  };
  std::vector<wired> to_object;

  auto flush_column = [&] {
    demo_bottom = std::max(demo_bottom, col_y);
    col_x += col_widest + 40;
    col_y = demo_top;
    col_widest = 0;
    rows_in_col = 0;
  };

  for(std::size_t k = 0; k < controls.size(); ++k)
  {
    if(rows_in_col >= rows_per_col)
      flush_column();

    const std::size_t i = controls[k];
    const port_t& c = m.inputs[i];
    const int inlet = t.pd_inlet[i];

    const pd_driver d = pd_emit_control(p, c, col_x, col_y, inlet);
    if(d.source >= 0)
      to_object.push_back({d.source, d.inlet});

    // Label to the right of the widget, vertically centred on it.
    const std::string label = one_line(port_label(c));
    const int label_cols = std::min<int>(46, std::max<int>(12, (int)label.size()));
    const auto ls = pd_text_size(label, label_cols);
    const int lx = col_x + d.width + label_gap;
    const int ly = col_y + std::max(0, (d.height - ls.h) / 2);
    p.text(lx, ly, label, label_cols);

    const int row_h = std::max(d.height, ls.h);
    col_widest = std::max(col_widest, d.width + label_gap + ls.w);
    col_y += row_h + row_gap;
    rows_in_col++;
  }
  flush_column();
  y = std::max(demo_bottom, demo_top);

  // --- messages -----------------------------------------------------------
  int msg_x = left;
  for(const auto& msg : m.messages)
  {
    std::string body = selector(msg.name);
    for(const auto& a : msg.arguments)
    {
      if(a == "float" || a == "double")
        body += " 0.5";
      else if(a == "int" || a == "bool")
        body += " 1";
      else if(a == "string" || a == "symbol" || a == "std::string")
        body += " sym";
    }
    const auto s = pd_box_size(body);
    if(msg_x + s.w > left + 540)
    {
      msg_x = left;
      y += s.h + 8;
    }
    const int mi = p.msg(msg_x, y, body);
    to_object.push_back({mi, 0});
    msg_x += s.w + 12;
  }
  if(!m.messages.empty())
    y += PD_FH + 6 + 8;

  y += 16;

  // --- audio sources ------------------------------------------------------
  int obj_y = y;
  std::vector<int> audio_sources;
  if(t.pd_signal_inlets > 0)
  {
    int ax = left;
    for(int k = 0; k < t.pd_signal_inlets; ++k)
    {
      const std::string body = k == 0 ? "osc~ 220" : "osc~ 330";
      const int o = p.obj(ax, y, body);
      audio_sources.push_back(o);
      ax += pd_box_size(body).w + 14;
    }
    obj_y = y + PD_FH + 6 + 20;
  }

  // --- the object ---------------------------------------------------------
  const int obj_idx = p.obj(left, obj_y, pd_escape(create));
  for(std::size_t k = 0; k < audio_sources.size(); ++k)
    p.connect(audio_sources[k], 0, obj_idx, static_cast<int>(k));
  for(const auto& w : to_object)
    p.connect(w.source, 0, obj_idx, w.inlet);

  y = obj_y + pd_box_size(create).h + 20;

  // --- outputs ------------------------------------------------------------
  int ox = left;
  int row_bottom = y;
  auto advance = [&](int w, int h) {
    ox += w + 16;
    row_bottom = std::max(row_bottom, y + h);
    if(ox > left + 520)
    {
      ox = left;
      y = row_bottom + 12;
      row_bottom = y;
    }
  };

  if(t.pd_signal_outlets > 0)
  {
    // vanilla's own idiom: clip the signal, scale it, then to the DAC.
    int dac = -1;
    for(int k = 0; k < t.pd_signal_outlets; ++k)
    {
      const std::string body = "clip~ -0.95 0.95";
      const int clip = p.obj(ox, y, body);
      p.connect(obj_idx, k, clip, 0);
      const auto cs = pd_box_size(body);
      if(dac < 0)
      {
        dac = p.obj(left, y + cs.h + 24, "dac~");
        row_bottom = std::max(row_bottom, y + cs.h + 24 + PD_FH + 6);
      }
      p.connect(clip, 0, dac, k % 2);
      ox += cs.w + 16;
    }
    y = row_bottom + 12;
    ox = left;
    row_bottom = y;
  }

  for(std::size_t i = 0; i < m.outputs.size(); ++i)
  {
    const auto& o = m.outputs[i];
    if(o.type == "audio")
      continue;
    const pd_sink s = pd_emit_sink(p, o, ox, y);
    if(s.box < 0)
      continue;
    p.connect(obj_idx, t.pd_outlet[i], s.box, 0);
    // Name the sink so the user knows which outlet it belongs to.
    const std::string tag = std::to_string(t.pd_outlet[i]) + ") " + o.name;
    const int tag_cols = std::max<int>(6, std::min<int>(24, (int)tag.size()));
    const auto ts = pd_text_size(tag, tag_cols);
    p.text(ox, y + s.height + 2, tag, tag_cols);
    advance(std::max(s.width, ts.w), s.height + 2 + ts.h);
  }
  y = row_bottom + 16;

  // --- footer: the reference subpatch + see-also --------------------------
  p.rule(left, y, 540);
  y += 12;

  int ref_w = 640, ref_h = 480;
  const std::string ref = pd_reference_body(m, t, create, ref_w, ref_h);
  const int ref_idx = p.subpatch(left, y, "reference", ref_w, ref_h, ref);
  (void)ref_idx;
  p.text(left + 110, y + 2, "<= click to open the full reference", 40);
  y += PD_FH + 6 + 10;

  if(!other_in.empty())
  {
    std::string s = "not representable in Pd: ";
    for(std::size_t k = 0; k < other_in.size(); ++k)
      s += (k ? ", " : "") + m.inputs[other_in[k]].name + " ("
           + m.inputs[other_in[k]].type + ")";
    y += p.text(left, y, s, PD_MAX_TEXT_COLS + 12);
  }

  const int width = std::max(620, p.max_x + 40);
  const int height = std::max(320, std::max(p.max_y, y) + 40);
  p.write(out, width, height);
}

// ---------------------------------------------------------------------------
// Max/MSP emitter
// ---------------------------------------------------------------------------
//
// .maxhelp is a JSON patcher (same schema family as .maxpat). Two things Max
// does differently from Pd drive this code:
//  - a `comment` is *not* auto-sized: it is drawn into patching_rect with the
//    saved `linecount`, so text longer than the rect is silently clipped. Every
//    comment here is measured and given a matching rect + linecount.
//  - inlets/outlets come from the binding, not from the port order: see
//    compute_topology().

// Arial 12 (the help-patch default) measures ~6.05 px per character. Size boxes
// with a slightly wider figure so nothing ever clips.
constexpr double MAX_CHAR_W = 7.0;
constexpr double MAX_LINE_H = 19.0;
constexpr int MAX_COMMENT_COLS = 46;

// A umenu's "items" is an array of *atoms* with "," entries separating the
// entries -- ["Up", ",", "Down", ",", "Up", "&", "Down"] is three items, the
// last one two words long. Handed a single space-separated string instead, Max
// reads one item whose label is the whole list, so the menu shows every choice
// at once rather than the selected one.
json max_umenu_items(const std::vector<std::string>& choices)
{
  json items = json::array();
  for(std::size_t i = 0; i < choices.size(); ++i)
  {
    if(i)
      items.push_back(",");
    // Multi-word labels are several atoms; an empty label would vanish.
    const std::string& c = choices[i];
    std::size_t start = 0;
    bool any = false;
    while(start < c.size())
    {
      const std::size_t sp = c.find(' ', start);
      const std::string word = c.substr(start, sp - start);
      if(!word.empty())
      {
        items.push_back(word);
        any = true;
      }
      if(sp == std::string::npos)
        break;
      start = sp + 1;
    }
    if(!any)
      items.push_back("<empty>");
  }
  return items;
}

struct max_patch
{
  json boxes = json::array();
  // Max draws the boxes array front-to-back, so a decoration emitted before the
  // text it sits behind hides it. Background boxes are collected separately and
  // appended last.
  json background = json::array();
  json lines = json::array();
  int n = 0;
  double max_x = 0, max_y = 0;

  void extend(double x, double y, double w, double h)
  {
    max_x = std::max(max_x, x + w);
    max_y = std::max(max_y, y + h);
  }

  std::string box(
      std::string_view maxclass, std::string_view text, double x, double y,
      double w, double h, int nin = 1, int nout = 1, json extra = json::object())
  {
    std::string id = "obj-" + std::to_string(++n);
    json b;
    b["id"] = id;
    b["maxclass"] = maxclass;
    b["numinlets"] = nin;
    b["numoutlets"] = nout;
    b["patching_rect"] = {x, y, w, h};
    b["fontname"] = "Arial";
    b["fontsize"] = 12.0;
    if(!text.empty())
      b["text"] = text;
    if(nout > 0)
      b["outlettype"] = std::vector<std::string>(static_cast<std::size_t>(nout), "");
    for(auto it = extra.begin(); it != extra.end(); ++it)
      b[it.key()] = it.value();
    if(maxclass == "panel")
      background.push_back(json{{"box", b}});
    else
      boxes.push_back(json{{"box", b}});
    extend(x, y, w, h);
    return id;
  }

  // A measured comment: the rect and linecount always match the text, so Max
  // never clips it.
  std::string comment(
      std::string_view text, double x, double y, int cols = MAX_COMMENT_COLS,
      json extra = json::object())
  {
    const int lines = wrapped_lines(text, cols);
    const int used
        = lines == 1 ? static_cast<int>(text.size()) : cols;
    const double w = std::max(24.0, used * MAX_CHAR_W + 8.0);
    const double h = lines * MAX_LINE_H + 4.0;
    if(lines > 1)
      extra["linecount"] = lines;
    return box("comment", text, x, y, w, h, 1, 0, std::move(extra));
  }

  double comment_h(std::string_view text, int cols = MAX_COMMENT_COLS) const
  {
    return wrapped_lines(text, cols) * MAX_LINE_H + 4.0;
  }
  double comment_w(std::string_view text, int cols = MAX_COMMENT_COLS) const
  {
    const int lines = wrapped_lines(text, cols);
    const int used = lines == 1 ? static_cast<int>(text.size()) : cols;
    return std::max(24.0, used * MAX_CHAR_W + 8.0);
  }

  void line(const std::string& s, int so, const std::string& d, int di)
  {
    if(s.empty() || d.empty() || so < 0 || di < 0)
      return;
    lines.push_back(
        json{{"patchline", {{"source", {s, so}}, {"destination", {d, di}}}}});
  }

  void write(std::ostream& o, double w, double h) const
  {
    json p;
    p["fileversion"] = 1;
    p["appversion"]
        = {{"major", 8},   {"minor", 5},     {"revision", 0},
           {"architecture", "x64"}, {"modernui", 1}};
    p["classnamespace"] = "box";
    p["rect"] = {80.0, 80.0, w, h};
    p["bglocked"] = 0;
    p["openinpresentation"] = 0;
    p["default_fontsize"] = 12.0;
    p["default_fontface"] = 0;
    p["default_fontname"] = "Arial";
    p["gridonopen"] = 1;
    p["gridsize"] = {15.0, 15.0};
    p["gridsnaponopen"] = 1;
    p["objectsnaponopen"] = 1;
    p["statusbarvisible"] = 2;
    p["toolbarvisible"] = 1;
    p["enablehscroll"] = 1;
    p["enablevscroll"] = 1;
    p["devicewidth"] = 0.0;
    p["description"] = "";
    p["digest"] = "";
    p["tags"] = "";
    p["style"] = "";
    p["subpatcher_template"] = "";
    p["assistshowspatchername"] = 0;
    json all = boxes;
    for(const auto& b : background)
      all.push_back(b);
    p["boxes"] = all;
    p["lines"] = lines;
    json doc;
    doc["patcher"] = p;
    o << doc.dump(2) << '\n';
  }
};

// The Max UI object that drives one control, and the box whose outlet 0 carries
// its value into the object.
struct max_driver
{
  std::string source;
  int inlet = 0;
  double width = 0, height = 0;
};

max_driver max_emit_control(max_patch& p, const port_t& c, double x, double y, int inlet)
{
  const std::string sel = selector(c.name);
  max_driver d;
  d.inlet = inlet >= 0 ? inlet : 0;

  std::string widget;
  double w = 0, h = 0;

  if(c.is_attribute)
  {
    // Attributes have a first-class editor in Max: attrui drives them by name,
    // no message plumbing needed. It talks to the object's left inlet.
    const std::string a = p.box(
        "attrui", "", x, y, 260, 23, 1, 1,
        json{{"attr", sel}, {"parameter_enable", 0}});
    d.source = a;
    d.inlet = 0;
    d.width = 260;
    d.height = 23;
    return d;
  }

  if(is_impulse(c))
  {
    // See the Pd emitter: an impulse is engaged by a bare "<name>" message, and
    // a "$1" message box fed a bang is an error.
    const double bw = std::max(60.0, sel.size() * MAX_CHAR_W + 12.0);
    const std::string mb = p.box("message", sel, x, y, bw, 22);
    d.source = mb;
    d.inlet = 0;
    d.width = bw;
    d.height = 22;
    return d;
  }
  else if(is_bool_like(c))
  {
    widget = p.box("toggle", "", x, y, 24, 24);
    w = 24;
    h = 24;
  }
  else if(!c.choices.empty())
  {
    widget = p.box(
        "umenu", "", x, y, 140, 22, 1, 3,
        json{{"items", max_umenu_items(c.choices)}, {"parameter_enable", 0}});
    w = 140;
    h = 22;
  }
  else if(c.value_type == "int" || c.value_type == "enum")
  {
    widget = p.box("number", "", x, y, 60, 22);
    w = 60;
    h = 22;
  }
  else if(c.value_type == "float")
  {
    if(c.range && c.range->min && c.range->max)
    {
      const double lo = *c.range->min, hi = *c.range->max;
      widget = p.box(
          "slider", "", x, y, 140, 22, 1, 1,
          json{{"floatoutput", 1},
               {"size", hi - lo},
               {"min", lo},
               {"orientation", 1},
               {"parameter_enable", 0}});
      w = 140;
      h = 22;
    }
    else
    {
      widget = p.box("flonum", "", x, y, 60, 22);
      w = 60;
      h = 22;
    }
  }
  else if(c.value_type == "string")
  {
    const std::string body = sel + " symbol";
    const double bw = std::max(80.0, body.size() * MAX_CHAR_W + 12.0);
    const std::string mb = p.box("message", body, x, y, bw, 22);
    d.source = mb;
    d.inlet = 0;
    d.width = bw;
    d.height = 22;
    return d;
  }
  else if(const int nc = component_count(c); nc > 0)
  {
    std::string body = sel;
    for(int i = 0; i < nc; i++)
      body += " " + component_default(c);
    const double bw = std::max(80.0, body.size() * MAX_CHAR_W + 12.0);
    const std::string mb = p.box("message", body, x, y, bw, 22);
    d.source = mb;
    d.inlet = 0;
    d.width = bw;
    d.height = 22;
    return d;
  }
  else
  {
    const double bw = std::max(80.0, sel.size() * MAX_CHAR_W + 12.0);
    const std::string mb = p.box("message", sel, x, y, bw, 22);
    d.source = mb;
    d.inlet = 0;
    d.width = bw;
    d.height = 22;
    return d;
  }

  if(inlet >= 0)
  {
    d.source = widget;
    d.width = w;
    d.height = h;
  }
  else
  {
    const std::string body = sel + " $1";
    const double bw = std::max(80.0, body.size() * MAX_CHAR_W + 12.0);
    const std::string mb = p.box("message", body, x + w + 12, y, bw, 22);
    p.line(widget, 0, mb, 0);
    d.source = mb;
    d.inlet = 0;
    d.width = w + 12 + bw;
    d.height = std::max(h, 22.0);
  }
  return d;
}

struct max_sink
{
  std::string box;
  double width = 0, height = 0;
};

max_sink max_emit_sink(max_patch& p, const port_t& o, double x, double y)
{
  max_sink s;
  const std::string sel = selector(o.name);

  auto value_box = [&](double px, double py) -> std::pair<std::string, size_t2> {
    if(o.value_type == "bool")
      return {p.box("toggle", "", px, py, 24, 24), {24, 24}};
    if(o.value_type == "int")
      return {p.box("number", "", px, py, 60, 22), {60, 22}};
    if(o.value_type == "string" || o.value_type == "enum")
      return {p.box("comment", "", px, py, 120, 22, 1, 0), {120, 22}};
    if(component_count(o) > 0)
      return {p.box("multislider", "", px, py, 140, 60, 1, 2), {140, 60}};
    return {p.box("flonum", "", px, py, 60, 22), {60, 22}};
  };

  if(o.type == "audio")
    return s;

  if(o.type == "message" || o.type == "callback" || o.type == "midi")
  {
    const std::string body = "print " + sel;
    const double bw = std::max(80.0, body.size() * MAX_CHAR_W + 12.0);
    s.box = p.box("newobj", body, x, y, bw, 22, 1, 0);
    s.width = bw;
    s.height = 22;
    return s;
  }

  if(!o.out_selector.empty())
  {
    const std::string body = "route " + selector(o.out_selector);
    const double bw = std::max(80.0, body.size() * MAX_CHAR_W + 12.0);
    const std::string r = p.box("newobj", body, x, y, bw, 22, 1, 2);
    auto [vb, vs] = value_box(x, y + 32);
    p.line(r, 0, vb, 0);
    s.box = r;
    s.width = std::max<double>(bw, vs.w);
    s.height = 32 + vs.h;
    return s;
  }

  auto [vb, vs] = value_box(x, y);
  s.box = vb;
  s.width = vs.w;
  s.height = vs.h;
  return s;
}

void emit_max(const model_t& m, std::ostream& out, const std::string& external_name)
{
  const topology t = compute_topology(m);
  max_patch p;

  std::vector<std::size_t> controls, audio_in_idx, matrix_in, other_in;
  for(std::size_t i = 0; i < m.inputs.size(); ++i)
  {
    const auto& c = m.inputs[i];
    if(c.type == "parameter")
      controls.push_back(i);
    else if(c.type == "audio")
      audio_in_idx.push_back(i);
    else if(t.is_max_jitter && c.is_matrix())
      matrix_in.push_back(i); // the object's jit_matrix inlet, not "unsupported"
    else
      other_in.push_back(i);
  }

  const std::string base = !external_name.empty() ? external_name
                           : m.c_name.empty()     ? m.name
                                                  : m.c_name;
  const std::string create = base + default_init_args(m);

  const double left = 30;
  double y = 20;

  // --- header -------------------------------------------------------------
  p.box("panel", "", left - 10, y - 8, 640, 62, 1, 0,
        json{{"bgfillcolor_type", "color"},
             {"bgfillcolor_color", {0.9, 0.9, 0.9, 1.0}},
             {"rounded", 6},
             {"border", 0}});
  p.comment(m.name, left, y, 60,
            json{{"fontsize", 18.0}, {"fontface", 1}});
  y += 26;
  {
    const std::string b = blurb(m, t);
    p.comment(b, left, y, 78);
    y += p.comment_h(b, 78);
  }
  y += 18;

  // --- controls -----------------------------------------------------------
  constexpr int rows_per_col = 14;
  const double row_gap = 12;
  const double label_gap = 14;
  const double demo_top = y;

  double col_x = left;
  double col_y = demo_top;
  double col_widest = 0;
  int rows_in_col = 0;
  double demo_bottom = demo_top;

  struct wired
  {
    std::string source;
    int inlet;
  };
  std::vector<wired> to_object;

  auto flush_column = [&] {
    demo_bottom = std::max(demo_bottom, col_y);
    col_x += col_widest + 44;
    col_y = demo_top;
    col_widest = 0;
    rows_in_col = 0;
  };

  for(std::size_t k = 0; k < controls.size(); ++k)
  {
    if(rows_in_col >= rows_per_col)
      flush_column();

    const std::size_t i = controls[k];
    const port_t& c = m.inputs[i];
    const max_driver d = max_emit_control(p, c, col_x, col_y, t.max_inlet[i]);
    if(!d.source.empty())
      to_object.push_back({d.source, d.inlet});

    const std::string label = port_label(c);
    const int cols = std::min<int>(MAX_COMMENT_COLS, std::max<int>(10, (int)label.size()));
    const double lw = p.comment_w(label, cols);
    const double lh = p.comment_h(label, cols);
    p.comment(label, col_x + d.width + label_gap,
              col_y + std::max(0.0, (d.height - lh) / 2), cols,
              json{{"textcolor", {0.3, 0.3, 0.3, 1.0}}});

    col_widest = std::max(col_widest, d.width + label_gap + lw);
    col_y += std::max(d.height, lh) + row_gap;
    rows_in_col++;
  }
  flush_column();
  y = std::max(demo_bottom, demo_top);

  // --- messages -----------------------------------------------------------
  double mx = left;
  double msg_bottom = y;
  for(const auto& msg : m.messages)
  {
    std::string body = selector(msg.name);
    for(const auto& a : msg.arguments)
    {
      if(a == "float" || a == "double")
        body += " 0.5";
      else if(a == "int" || a == "bool")
        body += " 1";
      else if(a == "string" || a == "symbol" || a == "std::string")
        body += " sym";
    }
    const double bw = std::max(60.0, body.size() * MAX_CHAR_W + 12.0);
    if(mx + bw > left + 620)
    {
      mx = left;
      y = msg_bottom + 10;
    }
    const std::string mb = p.box("message", body, mx, y, bw, 22);
    to_object.push_back({mb, 0});
    mx += bw + 12;
    msg_bottom = std::max(msg_bottom, y + 22);
  }
  y = msg_bottom + 26;

  // --- audio source -------------------------------------------------------
  // Max's audio binding builds a single multichannel signal inlet, so one
  // source feeds the object regardless of how many audio ports it declares.
  std::string audio_src;
  double obj_y = y;
  if(!audio_in_idx.empty())
  {
    const int nch = pd_signal_count([&] {
      std::vector<const port_t*> v;
      for(auto i : audio_in_idx)
        v.push_back(&m.inputs[i]);
      return v;
    }());
    const std::string body
        = nch > 1 ? "mc.cycle~ " + std::to_string(nch) + " 220" : "cycle~ 220";
    const double bw = std::max(90.0, body.size() * MAX_CHAR_W + 12.0);
    audio_src = p.box("newobj", body, left, y, bw, 22);
    obj_y = y + 46;
  }

  // --- Jitter source ------------------------------------------------------
  // A matrix operator is useless without a picture: drive it from a
  // [jit.noise] (or just a metro, for a generator) into the matrix inlet 0 and
  // show the result in a [jit.pwindow] below. A toggle starts the clock, so the
  // patch does something the moment the user clicks it.
  std::string matrix_src;
  if(t.is_max_jitter)
  {
    const std::string tgl = p.box("toggle", "", left, y, 24, 24);
    const std::string met = p.box("newobj", "qmetro 50", left + 34, y, 90, 22);
    p.line(tgl, 0, met, 0);
    double sy = y + 34;
    if(!matrix_in.empty())
    {
      const std::string body = "jit.noise 4 char 320 240";
      matrix_src = p.box(
          "newobj", body, left, sy, body.size() * MAX_CHAR_W + 12.0, 22);
      p.line(met, 0, matrix_src, 0);
      obj_y = sy + 46;
    }
    else
    {
      // Generator: the bang itself cooks the object.
      matrix_src = met;
      obj_y = sy + 12;
    }
  }

  // --- the object ---------------------------------------------------------
  int obj_inlets = 1, obj_outlets = 1;
  for(std::size_t i = 0; i < m.inputs.size(); ++i)
    obj_inlets = std::max(obj_inlets, t.max_inlet[i] + 1);
  for(std::size_t i = 0; i < m.outputs.size(); ++i)
    obj_outlets = std::max(obj_outlets, t.max_outlet[i] + 1);

  const double obj_w = std::max(160.0, create.size() * MAX_CHAR_W + 20.0);
  const std::string obj
      = p.box("newobj", create, left, obj_y, obj_w, 22, obj_inlets, obj_outlets);
  if(!audio_src.empty())
    p.line(audio_src, 0, obj, 0);
  if(!matrix_src.empty())
    p.line(matrix_src, 0, obj, 0); // matrix (or the cooking bang) on inlet 0
  for(const auto& w : to_object)
    p.line(w.source, 0, obj, w.inlet);

  y = obj_y + 46;

  // --- outputs ------------------------------------------------------------
  double ox = left;
  double row_bottom = y;

  // Matrix outputs go to a [jit.pwindow] so the result is actually visible.
  for(std::size_t i = 0; i < m.outputs.size() && t.is_max_jitter; ++i)
  {
    if(!m.outputs[i].is_matrix())
      continue;
    const std::string pw = p.box(
        "jit.pwindow", "", ox, y, 322, 242, 1, 2,
        json{{"rounded", 0}});
    p.line(obj, t.max_outlet[i], pw, 0);
    const std::string tag
        = std::to_string(t.max_outlet[i]) + ") " + m.outputs[i].name + " - matrix";
    const int cols = std::max<int>(6, std::min<int>(34, (int)tag.size()));
    p.comment(tag, ox, y + 246, cols, json{{"textcolor", {0.3, 0.3, 0.3, 1.0}}});
    row_bottom = std::max(row_bottom, y + 246 + p.comment_h(tag, cols));
    ox += 340;
  }
  if(ox > left)
  {
    ox = left;
    y = row_bottom + 16;
    row_bottom = y;
  }

  if(!m.outputs.empty())
  {
    bool any_audio = false;
    for(const auto& o : m.outputs)
      any_audio |= o.type == "audio";
    if(any_audio)
    {
      const std::string g = p.box("newobj", "*~ 0.2", ox, y, 70, 22);
      p.line(obj, 0, g, 0);
      const std::string dac = p.box("newobj", "ezdac~", ox, y + 40, 45, 45, 2, 0);
      p.line(g, 0, dac, 0);
      p.line(g, 0, dac, 1);
      const std::string sc = p.box("newobj", "scope~", ox + 90, y, 130, 90, 2, 0);
      p.line(g, 0, sc, 0);
      row_bottom = std::max(row_bottom, y + 90);
      ox += 240;
    }
    for(std::size_t i = 0; i < m.outputs.size(); ++i)
    {
      const auto& o = m.outputs[i];
      if(o.type == "audio")
        continue;
      if(t.is_max_jitter && o.is_matrix())
        continue; // already shown in a jit.pwindow
      if(ox > left + 560)
      {
        ox = left;
        y = row_bottom + 16;
        row_bottom = y;
      }
      const max_sink s = max_emit_sink(p, o, ox, y);
      if(s.box.empty())
        continue;
      p.line(obj, t.max_outlet[i], s.box, 0);
      const std::string tag = std::to_string(t.max_outlet[i]) + ") " + o.name;
      const int cols = std::max<int>(6, std::min<int>(28, (int)tag.size()));
      p.comment(tag, ox, y + s.height + 4, cols,
                json{{"textcolor", {0.3, 0.3, 0.3, 1.0}}});
      row_bottom = std::max(row_bottom, y + s.height + 4 + p.comment_h(tag, cols));
      ox += std::max(s.width, p.comment_w(tag, cols)) + 20;
    }
  }
  y = row_bottom + 26;

  // --- reference ----------------------------------------------------------
  auto section = [&](std::string_view title) {
    p.comment(title, left, y, 40,
              json{{"fontface", 1}, {"fontsize", 13.0}});
    y += p.comment_h(title, 40) + 4;
  };
  auto entry = [&](const std::string& s) {
    const int cols = 78;
    p.comment(s, left + 14, y, cols);
    y += p.comment_h(s, cols) + 2;
  };

  section("Inlets");
  if(t.is_audio)
    entry("inlet 0: multichannel signal. Controls are set with a "
          "\"<name> <value>\" message on it.");
  if(t.is_max_jitter)
    entry("This is a Jitter matrix operator: inlet 0 takes a jit_matrix, and "
          "controls are set with a \"<name> <value>\" message on it.");
  for(std::size_t i = 0; i < m.inputs.size(); ++i)
  {
    const auto& c = m.inputs[i];
    if(c.type == "audio")
      continue;
    std::string line;
    if(t.is_max_jitter && c.is_matrix())
      line = "inlet 0 (jit_matrix): ";
    else if(c.is_attribute)
      line = "@" + selector(c.name) + " (attribute): ";
    else if(t.max_inlet[i] >= 0)
      line = "inlet " + std::to_string(t.max_inlet[i]) + ": ";
    else
      line = "left inlet: ";
    line += port_label(c);
    entry(line);
  }

  if(!m.messages.empty())
  {
    y += 8;
    section("Messages");
    for(const auto& msg : m.messages)
    {
      std::string line = selector(msg.name);
      for(const auto& a : msg.arguments)
        line += " <" + a + ">";
      entry(line);
    }
  }

  if(!m.outputs.empty())
  {
    y += 8;
    section("Outlets");
    for(std::size_t i = 0; i < m.outputs.size(); ++i)
    {
      const auto& o = m.outputs[i];
      std::string line = "outlet " + std::to_string(t.max_outlet[i]) + ": "
                         + port_label(o);
      if(!o.out_selector.empty())
        line += " (prefixed with '" + selector(o.out_selector) + "')";
      entry(line);
    }
  }

  if(!m.init_arguments.empty())
  {
    y += 8;
    section("Arguments");
    for(std::size_t i = 0; i < m.init_arguments.size(); ++i)
      entry(std::to_string(i + 1) + ") " + m.init_arguments[i]);
  }

  if(!other_in.empty())
  {
    y += 8;
    section("Not representable in Max");
    for(auto i : other_in)
      entry(m.inputs[i].name + " - " + m.inputs[i].type);
  }

  y += 8;
  {
    std::string about = "Generated by avendish";
    if(!m.author.empty())
      about += " - " + m.author;
    if(!m.version.empty())
      about += " - v" + m.version;
    p.comment(about, left, y, 60, json{{"textcolor", {0.5, 0.5, 0.5, 1.0}}});
    y += p.comment_h(about, 60);
  }

  p.write(out, std::max(700.0, p.max_x + 40), std::max(500.0, p.max_y + 40));
}

// ---------------------------------------------------------------------------
// Godot text scene (.tscn) — fully text-emittable. Instantiates the
// extension-registered class and sets a few exported properties to their init
// values. `cls` is the registered class name (avnd_<c_name><suffix>); when
// absent we fall back to avnd_<c_name>.
// ---------------------------------------------------------------------------
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
  const topology t = compute_topology(m);
  const std::string klass
      = !cls.empty() ? cls : "avnd_" + (m.c_name.empty() ? m.name : m.c_name);

  out << "[gd_scene format=3]\n\n";
  out << "; " << m.name << " - " << blurb(m, t) << " (auto-generated example)\n\n";
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
  const topology t = compute_topology(m);
  const std::string ty = optype.empty() ? m.c_name : optype;
  out << "# TouchDesigner example builder for " << m.name << "\n";
  out << "# " << blurb(m, t) << "\n";
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
    out << "#   - " << c.name << " : " << port_typestr(c);
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
  const topology t = compute_topology(m);
  out << "#!/usr/bin/env python3\n";
  out << "\"\"\"" << m.name << " - " << blurb(m, t) << "\n\n";
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
    emit_pd(m, out, hint);
  else if(backend == "max" || backend == "maxhelp")
    emit_max(m, out, hint);
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
