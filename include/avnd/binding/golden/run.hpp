#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Golden-output generator: runs an avendish object offline with deterministic
// test input and writes golden/<c_name>.json (inputs + outputs + meta). The
// file is the oracle every backend binding must reproduce -- see
// OUTPUT_VERIFICATION_PLAN.md. Mirrors what a real binding does (the example
// host in binding/example/example_processor.hpp is the reference), kept to the
// subset needed to feed inputs and read outputs.

#include <avnd/binding/dump/json_writer.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/concepts/callback.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/message.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/wrappers/audio_channel_manager.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_storage.hpp>
#include <avnd/wrappers/prepare.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/process_execution.hpp>

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>

#include <tuple>

#include <cmath>
#include <cstdint>
#include <deque>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace golden
{
inline constexpr int k_seed = 1234;
inline constexpr int k_frames = 64;
inline constexpr double k_rate = 44100.0;

// Deterministic test signal for input audio channel `ch`, sample `i`.
inline float test_sample(int ch, int i, int frames)
{
  // Bounded, distinct per channel, no randomness.
  return 0.5f * std::sin(2.0 * 3.14159265358979 * (ch + 1) * (i + 1) / frames);
}

// Bytes per pixel for a halp texture (static member or method, else assume 4).
int tex_bpp(const auto& tex)
{
  using Tex = std::decay_t<decltype(tex)>;
  if constexpr(requires { tex.bytes_per_pixel(); })
    return tex.bytes_per_pixel();
  else if constexpr(requires { Tex::bytes_per_pixel; })
    return Tex::bytes_per_pixel;
  else
    return 4;
}

// --- container / aggregate control support ---------------------------------
// Deterministic element for slot k of a numeric container: integers count up
// from 1, floats spread over (0..1) -- bounded, distinct per slot, replayable
// by any backend as a plain list message.
template <typename ET>
ET container_element(int k)
{
  if constexpr(std::is_same_v<ET, bool>)
    return (k % 2) == 0;
  else if constexpr(std::is_integral_v<ET>)
    return static_cast<ET>(k + 1);
  else
    return static_cast<ET>(0.25 * (k + 1));
}

// Container classification helpers (plain constexpr functions: MSVC's parser
// is unreliable with nested-requires inside requires-expressions here).
// A resizable sequence of scalars (vector/list-ish).
template <typename VT>
constexpr bool scalar_sequence()
{
  if constexpr(requires(VT v) {
                 v.resize(3);
                 v[0];
               })
    return std::is_arithmetic_v<std::decay_t<decltype(std::declval<VT&>()[0])>>;
  else
    return false;
}

// A fixed-size indexable pack of scalars (std::array-ish).
template <typename VT>
constexpr bool scalar_fixed_array()
{
  if constexpr(scalar_sequence<VT>())
    return false;
  else if constexpr(requires(VT v) {
                      std::tuple_size<VT>::value;
                      v[0];
                    })
    return std::is_arithmetic_v<std::decay_t<decltype(std::declval<VT&>()[0])>>;
  else
    return false;
}

// An aggregate whose members are all scalars (xy / rgba / custom structs).
template <typename VT>
constexpr bool scalar_aggregate()
{
  if constexpr(
      std::is_aggregate_v<VT> && !scalar_sequence<VT>() && !scalar_fixed_array<VT>()
      && !avnd::iterable_ish<VT>)
  {
    if constexpr(avnd::pfr::tuple_size_v<VT> > 0)
      return boost::mp11::mp_all_of<avnd::as_typelist<VT>, std::is_arithmetic>::value;
    else
      return false;
  }
  else
    return false;
}

// A clean, matchable name: the port's own name if it has one, else a positional
// key (unnamed ports have no reflected name on compilers without std reflection,
// and get_name<F>() then yields a compiler-mangled placeholder).
template <typename F>
std::string clean_name(int index)
{
  std::string_view nm = avnd::get_name<F>();
  if(nm.empty() || nm.find('<') != std::string_view::npos
     || nm.find("unnamed") != std::string_view::npos)
    return "p" + std::to_string(index);
  return std::string{nm};
}

// One test case = override control #index with `value` (index < 0 = base case,
// all controls at default). We sweep one control at a time: base, then each enum
// mode and each numeric range endpoint. `value` is the enum ordinal or endpoint.
// kind extends the sweep beyond plain controls: `impulse` engages the targeted
// bang/impulse port for this case, `string_v` overrides a string control with
// `str`, and `message` invokes declared message port #index with deterministic
// arguments before processing (a backend replays it by sending the message,
// then triggering one processing pass).
struct case_override
{
  enum kind_t
  {
    control = 0,
    impulse,
    string_v,
    message
  };
  int index = -1;
  double value = 0.0;
  kind_t kind = control;
  std::string_view str{};
};

constexpr std::string_view case_kind_name(case_override::kind_t k)
{
  switch(k)
  {
    case case_override::impulse:
      return "impulse";
    case case_override::string_v:
      return "string";
    case case_override::message:
      return "message";
    default:
      return "control";
  }
}

// Set a deterministic value on an input control field (honouring `ov` when it
// targets this control), then append {index, name, value} to the ordered `arr`.
// Returns whether the field's value was actually written (callers then fire
// field.update(obj) exactly like a binding does when a control message lands).
template <typename F>
bool set_and_record_control(
    F& field, dump_json::document& jdoc, dump_json::value arr, int index,
    const case_override& ov)
{
  using vt = std::decay_t<decltype(field.value)>;
  const bool over = (ov.index == index);
  auto node = jdoc.make_node();
  node["index"] = index;
  node["name"] = clean_name<F>(index);
  if constexpr(std::is_same_v<vt, bool>)
  {
    field.value = over ? (ov.value != 0.0) : true;
    node["value"] = (bool)field.value;
  }
  else if constexpr(std::floating_point<vt>)
  {
    if(over)
      field.value = static_cast<vt>(ov.value);
    else if constexpr(avnd::parameter_with_minmax_range<F>)
    {
      constexpr auto r = avnd::get_range<F>();
      field.value = static_cast<vt>(r.min + 0.5 * (r.max - r.min));
    }
    else
      field.value = static_cast<vt>(0.5);
    node["value"] = field.value;
  }
  else if constexpr(std::is_enum_v<vt>)
  {
    field.value = over ? static_cast<vt>((int)ov.value) : static_cast<vt>(0);
    node["value"] = static_cast<int>(field.value);
  }
  else if constexpr(std::integral<vt>)
  {
    if(over)
      field.value = static_cast<vt>((std::int64_t)ov.value);
    else if constexpr(avnd::parameter_with_minmax_range<F>)
    {
      constexpr auto r = avnd::get_range<F>();
      field.value = static_cast<vt>(r.min + (r.max - r.min) / 2);
    }
    else
      field.value = static_cast<vt>(1);
    node["value"] = field.value;
  }
  else if constexpr(std::convertible_to<vt, std::string_view>)
  {
    if(over && ov.kind == case_override::string_v)
      field.value = vt{ov.str};
    else
      field.value = "test";
    node["value"] = std::string_view{field.value};
  }
  else if constexpr(requires {
                      field.value.reset();
                      field.value.emplace();
                    })
  {
    // Impulse / bang-like port (optional value). Engaged only when this case
    // targets it; "bang" tells a backend harness to send a bang to this port.
    if(over && ov.kind == case_override::impulse)
    {
      field.value.emplace();
      node["value"] = "bang";
    }
    else
    {
      field.value.reset();
      node["value"] = "unrecorded";
    }
  }
  else if constexpr(scalar_sequence<vt>())
  {
    // Vector/list of scalars: three deterministic elements, recorded as a
    // JSON array (a backend replays it as a list message).
    field.value.resize(3);
    for(int k = 0; k < 3; k++)
      field.value[k] = container_element<std::decay_t<decltype(field.value[0])>>(k);
    node["value"] = field.value;
  }
  else if constexpr(scalar_fixed_array<vt>())
  {
    constexpr int N = std::tuple_size<vt>::value;
    for(int k = 0; k < N; k++)
      field.value[k] = container_element<std::decay_t<decltype(field.value[0])>>(k);
    node["value"] = field.value;
  }
  else if constexpr(scalar_aggregate<vt>())
  {
    // Aggregate of scalars (xy / rgba / ...): one deterministic value per
    // member, recorded as a JSON array in declaration order.
    auto varr = node["value"];
    varr.ensure_array();
    int k = 0;
    avnd::pfr::for_each_field(field.value, [&](auto& m) {
      m = container_element<std::decay_t<decltype(m)>>(k++);
      varr.push_back(m);
    });
  }
  else
  {
    node["value"] = "unrecorded"; // opaque type: structural compare only
    arr.push_back(node);
    return false;
  }
  arr.push_back(node);
  return true;
}

// --- message-port support -------------------------------------------------
// A message case invokes a declared message port with deterministic arguments,
// mirroring what a backend does when the object receives e.g. [add 2( in Pd.
// Only signatures whose (non-self) arguments are all scalars or strings are
// enumerated; anything else has no portable deterministic stimulus.

template <typename A>
using golden_msg_arg_ok = std::bool_constant<
    std::is_arithmetic_v<std::decay_t<A>>
    || std::is_same_v<std::decay_t<A>, std::string>
    || std::is_same_v<std::decay_t<A>, std::string_view>>;

// The argument list a caller must synthesize: the message's reflected argument
// list, minus a leading T& ("self") argument if the function takes one.
template <typename T, typename L, bool Empty = boost::mp11::mp_size<L>::value == 0>
struct golden_msg_args
{
  using type = L;
};
template <typename T, typename L>
struct golden_msg_args<T, L, false>
{
  using first = std::decay_t<boost::mp11::mp_first<L>>;
  using type = std::conditional_t<
      std::is_same_v<first, T>, boost::mp11::mp_pop_front<L>, L>;
};
template <typename T, typename L>
using golden_msg_args_t = typename golden_msg_args<T, L>::type;

template <typename T, typename M>
consteval bool message_case_supported()
{
  if constexpr(std::is_void_v<avnd::message_reflection<M>>)
    return false;
  else
  {
    using args = golden_msg_args_t<T, typename avnd::message_reflection<M>::arguments>;
    return boost::mp11::mp_all_of<args, golden_msg_arg_ok>::value;
  }
}

// Deterministic value for one message argument.
template <typename Arg>
auto make_msg_arg()
{
  using A = std::decay_t<Arg>;
  if constexpr(std::is_same_v<A, bool>)
    return true;
  else if constexpr(std::floating_point<A>)
    return A(0.5);
  else if constexpr(std::is_arithmetic_v<A>)
    return A(2);
  else
    return std::string("msg");
}

// Invoke message M on `impl` with the given args, trying the same call shapes
// the pd/max bindings do (functor, member function, free function; with or
// without a leading self argument).
template <typename M, typename T>
bool invoke_message_with_args(T& impl, auto&... args)
{
  if constexpr(requires(M m) { m(impl, args...); })
  {
    M{}(impl, args...);
    return true;
  }
  else if constexpr(requires(M m) { m(args...); })
  {
    M{}(args...);
    return true;
  }
  else
  {
    static constexpr auto f = avnd::message_get_func<M>();
    if constexpr(std::is_member_function_pointer_v<std::decay_t<decltype(f)>>)
    {
      if constexpr(requires { (impl.*f)(args...); })
      {
        (impl.*f)(args...);
        return true;
      }
      else if constexpr(requires { (impl.*f)(impl, args...); })
      {
        (impl.*f)(impl, args...);
        return true;
      }
      else
        return false;
    }
    else
    {
      if constexpr(requires { f(impl, args...); })
      {
        f(impl, args...);
        return true;
      }
      else if constexpr(requires { f(args...); })
      {
        f(args...);
        return true;
      }
      else
        return false;
    }
  }
}

// Invoke message #target with deterministic args; append {index, name, args}
// to the case's inputs.messages array so a backend can replay it.
template <typename T>
void run_message_case(
    avnd::effect_container<T>& effect, int target, dump_json::document& jdoc,
    dump_json::value in_node)
{
  if constexpr(avnd::messages_introspection<T>::size > 0)
  {
    int idx = 0;
    avnd::messages_introspection<T>::for_all(
        avnd::get_messages(effect), [&]<typename M>(M& field) {
          const int i = idx++;
          if(i != target)
            return;
          auto mnode = jdoc.make_node();
          mnode["index"] = i;
          mnode["name"] = clean_name<M>(i);
          auto args_arr = mnode["args"];
          args_arr.ensure_array();
          if constexpr(message_case_supported<T, M>())
          {
            using args_t
                = golden_msg_args_t<T, typename avnd::message_reflection<M>::arguments>;
            [&]<typename... Args>(boost::mp11::mp_list<Args...>*) {
              auto tup = std::tuple(make_msg_arg<Args>()...);
              std::apply(
                  [&](auto&... as) {
                    (args_arr.push_back(as), ...);
                    mnode["status"] = invoke_message_with_args<M>(effect.effect, as...)
                                          ? "invoked"
                                          : "uninvokable";
                  },
                  tup);
            }((args_t*)nullptr);
          }
          else
          {
            mnode["status"] = "unsupported";
          }
          auto msgs = in_node["messages"];
          msgs.ensure_array();
          msgs.push_back(mnode);
        });
  }
}

// Enumerate the OAT test cases for T: the base case, then for every enum input a
// case per non-zero mode, for every ranged numeric input a case at min and
// at max, a false case per bool, an alternate string per string input, an
// engaged case per impulse input, and one case per invocable message port.
// Bounded (sum, not product), so it stays small.
template <typename T>
std::vector<case_override> enumerate_cases()
{
  std::vector<case_override> cases;
  cases.push_back({-1, 0.0}); // base
  if constexpr(avnd::parameter_input_introspection<T>::size > 0)
  {
    avnd::effect_container<T> probe;
    avnd::init_controls(probe);
    int idx = 0;
    avnd::parameter_input_introspection<T>::for_all(
        avnd::get_inputs(probe), [&](auto& field) {
          using F = std::decay_t<decltype(field)>;
          using vt = std::decay_t<decltype(field.value)>;
          if constexpr(std::is_same_v<vt, bool>)
          {
            cases.push_back({idx, 0.0});
          }
          else if constexpr(std::is_enum_v<vt>)
          {
            constexpr int C = (int)avnd::get_enum_choices_count<F>();
            for(int c = 1; c < C; ++c)
              cases.push_back({idx, (double)c});
          }
          else if constexpr(
              (std::floating_point<vt> || std::integral<vt>)&&avnd::
                  parameter_with_minmax_range<F>)
          {
            constexpr auto r = avnd::get_range<F>();
            if((double)r.min != (double)r.max)
            {
              cases.push_back({idx, (double)r.min});
              cases.push_back({idx, (double)r.max});
            }
          }
          else if constexpr(std::convertible_to<vt, std::string_view>)
          {
            cases.push_back({idx, 0.0, case_override::string_v, "avnd"});
          }
          else if constexpr(requires {
                              field.value.reset();
                              field.value.emplace();
                            })
          {
            cases.push_back({idx, 1.0, case_override::impulse});
          }
          idx++;
        });
  }
  if constexpr(avnd::messages_introspection<T>::size > 0)
  {
    avnd::effect_container<T> probe;
    int idx = 0;
    avnd::messages_introspection<T>::for_all(
        avnd::get_messages(probe), [&]<typename M>(M&) {
          if constexpr(message_case_supported<T, M>())
            cases.push_back({idx, 0.0, case_override::message});
          idx++;
        });
  }
  return cases;
}

// Append an output control field's value as {index, name, value}.
template <typename F>
void record_output_control(F& field, dump_json::document& jdoc, dump_json::value arr, int index)
{
  using vt = std::decay_t<decltype(field.value)>;
  auto node = jdoc.make_node();
  node["index"] = index;
  node["name"] = clean_name<F>(index);
  if constexpr(std::is_same_v<vt, bool> || std::floating_point<vt> || std::integral<vt>)
    node["value"] = field.value;
  else if constexpr(std::is_enum_v<vt>)
    node["value"] = static_cast<int>(field.value);
  else if constexpr(std::convertible_to<vt, std::string_view>)
    node["value"] = std::string_view{field.value};
  else if constexpr(scalar_sequence<vt>() || scalar_fixed_array<vt>())
    node["value"] = field.value;
  else if constexpr(scalar_aggregate<vt>())
  {
    auto varr = node["value"];
    varr.ensure_array();
    avnd::pfr::for_each_field(field.value, [&](auto& m) { varr.push_back(m); });
  }
  else
    node["value"] = "unrecorded";
  arr.push_back(node);
}

// --- callback-output capture ------------------------------------------------
// Callback outputs (avnd::callback / view_callback) fire during processing; a
// binding turns each firing into an outlet message. Record every firing's
// arguments so backends can diff them. One recorder per callback port; the
// recorder appends one array-of-args per event into the port's `events` node.
struct callback_recorder
{
  dump_json::document* jdoc{};
  dump_json::value events;

  template <typename... Args>
  void fire(const Args&... args)
  {
    auto ev = jdoc->make_node();
    ev.ensure_array();
    (push_arg(ev, args), ...);
    events.push_back(ev);
  }

  void push_arg(dump_json::value arr, const auto& a)
  {
    using A = std::decay_t<decltype(a)>;
    auto n = jdoc->make_node();
    if constexpr(std::is_same_v<A, bool> || std::is_arithmetic_v<A>)
      n = a;
    else if constexpr(std::is_enum_v<A>)
      n = static_cast<int>(a);
    else if constexpr(std::convertible_to<A, std::string_view>)
      n = std::string_view{a};
    else
      n = "unrecorded";
    arr.push_back(n);
  }
};

template <typename R, typename... Args, template <typename...> typename F>
void wire_callback_view(callback_recorder& rec, F<R(Args...)>& call)
{
  call.context = &rec;
  call.function = [](void* ptr, Args... args) -> R {
    static_cast<callback_recorder*>(ptr)->fire(args...);
    if constexpr(!std::is_void_v<R>)
      return R{};
  };
}

template <typename R, typename... Args, template <typename...> typename F>
void wire_callback_func(callback_recorder& rec, F<R(Args...)>& call)
{
  call = [&rec](Args... args) -> R {
    rec.fire(args...);
    if constexpr(!std::is_void_v<R>)
      return R{};
  };
}

// Run ONE test case (fresh effect, override `ov` applied) and write its inputs
// and outputs into `case_node`. Throws on failure (caught by run()).
template <typename T>
void run_case(dump_json::document& jdoc, dump_json::value case_node, const case_override& ov)
{
    avnd::effect_container<T> effect;
    avnd::process_adapter<T> processor;
    avnd::audio_channel_manager<T> channels{effect};

    // Request a small, fixed channel layout (fixed-channel objects override it).
    channels.set_input_channels(effect, 0, 2);
    channels.set_output_channels(effect, 0, 2);
    avnd::init_controls(effect);

    // Install no-op handlers for host-provided callables so prepare()/process()
    // never call an empty std::function (that is a hard crash). Same set the
    // example host wires.
    if constexpr(avnd::audio_bus_input_introspection<T>::size > 0)
      avnd::audio_bus_input_introspection<T>::for_all(
          avnd::get_inputs(effect), [](auto& p) {
            if constexpr(requires { p.request_channels; })
              if(!p.request_channels)
                p.request_channels = [](int) {};
          });
    if constexpr(avnd::audio_bus_output_introspection<T>::size > 0)
      avnd::audio_bus_output_introspection<T>::for_all(
          avnd::get_outputs(effect), [](auto& p) {
            if constexpr(requires { p.request_channels; })
              if(!p.request_channels)
                p.request_channels = [](int) {};
          });

    // Output/input buffer ports push through buffer.upload -- give it a no-op.
    auto wire_buffer = [](auto& p) {
      if constexpr(requires { p.buffer.upload; })
        if(!p.buffer.upload)
          p.buffer.upload = [](const char*, std::int64_t, std::int64_t) {};
    };
    if constexpr(avnd::buffer_output_introspection<T>::size > 0)
      avnd::buffer_output_introspection<T>::for_all(avnd::get_outputs(effect), wire_buffer);
    if constexpr(avnd::buffer_input_introspection<T>::size > 0)
      avnd::buffer_input_introspection<T>::for_all(avnd::get_inputs(effect), wire_buffer);

    // Workers offload a job to the host; run it inline.
    if constexpr(avnd::has_worker<T>)
      for(auto& impl : effect.effects())
      {
        using worker_t = std::decay_t<decltype(impl.worker)>;
        impl.worker.request = [](auto&&... args) {
          worker_t::work(std::forward<decltype(args)>(args)...);
        };
      }

    const int in_N = channels.actual_runtime_inputs;
    const int out_N = channels.actual_runtime_outputs;

    avnd::process_setup setup{
        .input_channels = in_N,
        .output_channels = out_N,
        .frames_per_buffer = k_frames,
        .rate = k_rate};
    processor.allocate_buffers(setup, float{});
    effect.init_channels(in_N, out_N);
    avnd::prepare(effect, setup);

    // Sample-accurate control ports need their per-buffer storage reserved
    // before process() (else the object writes into unallocated storage).
    avnd::control_storage<T> control_buffers;
    if constexpr(sizeof(control_buffers) > 1)
      control_buffers.reserve_space(effect, k_frames);

    // Wire callback outputs to recorders (must happen before any message
    // invocation or processing so every firing is captured -- and so the
    // object never calls a null/empty callable).
    std::deque<callback_recorder> cb_recorders;
    std::vector<dump_json::value> cb_nodes;
    if constexpr(avnd::callback_output_introspection<T>::size > 0)
    {
      int cidx = 0;
      avnd::callback_output_introspection<T>::for_all(
          avnd::get_outputs(effect), [&]<typename C>(C& port) {
            auto node = jdoc.make_node();
            node["index"] = cidx;
            node["name"] = clean_name<C>(cidx);
            auto ev = node["events"];
            ev.ensure_array();
            auto& rec = cb_recorders.emplace_back(&jdoc, ev);
            using call_type = std::decay_t<decltype(port.call)>;
            if constexpr(avnd::function_view_ish<call_type>)
              wire_callback_view(rec, port.call);
            else if constexpr(avnd::function_ish<call_type>)
              wire_callback_func(rec, port.call);
            cb_nodes.push_back(node);
            cidx++;
          });
    }

    // --- inputs ---
    auto in = case_node["inputs"];
    auto in_controls = in["controls"];
    in_controls.ensure_array();
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      int idx = 0;
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(effect), [&](auto& field) {
            if(set_and_record_control(field, jdoc, in_controls, idx++, ov))
            {
              // Bindings fire the port's update hook when a control message
              // lands; mirror that (it may fire callbacks, recorded above).
              if constexpr(requires { field.update(effect.effect); })
                field.update(effect.effect);
            }
          });
    }

    // audio input buffers
    std::vector<std::vector<float>> inbuf(in_N, std::vector<float>(k_frames, 0.f));
    std::vector<std::vector<float>> outbuf(out_N, std::vector<float>(k_frames, 0.f));
    std::vector<float*> in_ptrs(in_N), out_ptrs(out_N);
    auto in_audio = in["audio"];
    in_audio.ensure_array();
    for(int c = 0; c < in_N; ++c)
    {
      for(int i = 0; i < k_frames; ++i)
        inbuf[c][i] = test_sample(c, i, k_frames);
      in_ptrs[c] = inbuf[c].data();
      auto chnode = jdoc.make_node();
      chnode = inbuf[c];
      in_audio.push_back(chnode);
    }
    for(int c = 0; c < out_N; ++c)
      out_ptrs[c] = outbuf[c].data();

    // CPU texture inputs: give each a small deterministic test image so filters
    // have something to read (storage must outlive process()). GPU textures are
    // not handled (they need a graphics context).
    std::deque<std::vector<unsigned char>> tex_storage;
    auto in_tex = in["texture"];
    in_tex.ensure_array();
    if constexpr(avnd::cpu_texture_input_introspection<T>::size > 0)
    {
      int tin = 0;
      avnd::cpu_texture_input_introspection<T>::for_all(
          avnd::get_inputs(effect), [&](auto& port) {
            const int w = 16, h = 16, bpp = tex_bpp(port.texture);
            auto& buf = tex_storage.emplace_back((std::size_t)w * h * bpp);
            for(std::size_t i = 0; i < buf.size(); ++i)
              buf[i] = (unsigned char)(i & 0xFF);
            port.texture.width = w;
            port.texture.height = h;
            port.texture.bytes
                = reinterpret_cast<decltype(port.texture.bytes)>(buf.data());
            if constexpr(requires { port.texture.changed; })
              port.texture.changed = true;
            // Record the fed image spec so a backend harness can synthesize
            // the identical input: byte i = i mod 256, row-major, no padding.
            auto node = jdoc.make_node();
            node["index"] = tin++;
            node["width"] = w;
            node["height"] = h;
            node["bytes_per_pixel"] = bpp;
            node["pattern"] = "mod256";
            in_tex.push_back(node);
          });
    }

    // Message case: deliver the message (like a backend receiving [name args()
    // right before the processing pass.
    if(ov.kind == case_override::message)
      run_message_case(effect, ov.index, jdoc, in);

    // --- run ---
    // Control objects taking a host tick have no audio path at all: the
    // process adapters would static_assert. Synthesize one processing pass,
    // like the pd/max bindings do on bang (frames = 1 so rate-dependent
    // state advances identically across backends).
    static constexpr bool tick_only_object
        = avnd::has_tick<T> && avnd::audio_input_introspection<T>::size == 0
          && avnd::audio_output_introspection<T>::size == 0;
    if constexpr(tick_only_object)
    {
      typename T::tick t{};
      if constexpr(requires { t.frames; })
        t.frames = 1;
      avnd::invoke_effect(effect, t);
    }
    else
    {
      processor.process(
          effect, avnd::span<float*>{in_ptrs.data(), (std::size_t)in_N},
          avnd::span<float*>{out_ptrs.data(), (std::size_t)out_N}, k_frames);
    }

    // --- outputs ---
    auto out = case_node["outputs"];
    auto out_controls = out["controls"];
    out_controls.ensure_array();
    if constexpr(avnd::parameter_output_introspection<T>::size > 0)
    {
      int idx = 0;
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(effect), [&](auto& field) {
            record_output_control(field, jdoc, out_controls, idx++);
          });
    }

    auto out_audio = out["audio"];
    out_audio.ensure_array();
    for(int c = 0; c < out_N; ++c)
    {
      auto chnode = jdoc.make_node();
      chnode = outbuf[c];
      out_audio.push_back(chnode);
    }

    // Callback outputs: every firing captured by the recorders wired above.
    auto out_cb = out["callbacks"];
    out_cb.ensure_array();
    for(auto& node : cb_nodes)
      out_cb.push_back(node);

    // CPU texture outputs: record dimensions + a content hash (exact pixels are
    // large; the hash is enough for a backend to match against).
    auto out_tex = out["texture"];
    out_tex.ensure_array();
    if constexpr(avnd::cpu_texture_output_introspection<T>::size > 0)
    {
      int tidx = 0;
      avnd::cpu_texture_output_introspection<T>::for_all(
          avnd::get_outputs(effect), [&](auto& port) {
            auto node = jdoc.make_node();
            node["index"] = tidx++;
            node["width"] = port.texture.width;
            node["height"] = port.texture.height;
            std::uint64_t hash = 1469598103934665603ULL;
            const int bpp = tex_bpp(port.texture);
            if(port.texture.bytes && port.texture.width > 0 && port.texture.height > 0)
            {
              const std::size_t n
                  = (std::size_t)port.texture.width * port.texture.height * bpp;
              const auto* b = reinterpret_cast<const unsigned char*>(port.texture.bytes);
              for(std::size_t i = 0; i < n; ++i)
              {
                hash ^= b[i];
                hash *= 1099511628211ULL;
              }
            }
            node["hash"] = (std::int64_t)hash;
            out_tex.push_back(node);
          });
    }
}

template <typename T>
int run(std::string_view path, std::string_view external_name = {})
{
  dump_json::document jdoc;
  auto root = jdoc.root();
  // The name backends register the external under (the CMake C_NAME) is what
  // differential harnesses look artifacts up by; the introspected c_name is
  // the fallback when the caller doesn't pass one.
  root["c_name"] = !external_name.empty() ? external_name : avnd::get_c_name<T>();
  auto meta = root["meta"];
  meta["seed"] = k_seed;
  meta["frames"] = k_frames;
  meta["rate"] = k_rate;

  auto finish = [&](std::string_view status) {
    root["meta"]["status"] = status;
    std::ofstream f{std::string{path}};
    f << jdoc.dump();
    return 0;
  };

  auto cases_arr = root["cases"];
  cases_arr.ensure_array();

  std::vector<case_override> overrides;
  try
  {
    overrides = enumerate_cases<T>();
  }
  catch(...)
  {
    // If we cannot even enumerate, fall back to the single base case.
    overrides = {{-1, 0.0}};
  }
  meta["num_cases"] = (int)overrides.size();

  bool all_ok = true;
  int ci = 0;
  for(const auto& ov : overrides)
  {
    auto cnode = jdoc.make_node();
    cnode["index"] = ci++;
    cnode["override_control"] = ov.index;
    cnode["override_value"] = ov.value;
    cnode["kind"] = case_kind_name(ov.kind);
    try
    {
      run_case<T>(jdoc, cnode, ov);
      cnode["status"] = "ok";
    }
    catch(const std::exception& e)
    {
      cnode["status"] = "error";
      cnode["error"] = std::string_view{e.what()};
      all_ok = false;
    }
    catch(...)
    {
      cnode["status"] = "error";
      all_ok = false;
    }
    cases_arr.push_back(cnode);
  }

  return finish(all_ok ? "ok" : "partial");
}
}
