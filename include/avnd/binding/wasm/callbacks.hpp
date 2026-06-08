#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Callback bridge for the WASM backend (plug-in -> JS events).
 *
 * Callbacks can fire on the audio thread, so we must not call into JS from
 * there: each emission is queued and drained later via drainCallbacks(). To
 * stay thread-correct we don't build the emscripten::val at emit time (a val
 * needs a JS context); instead we capture the native args by value into a
 * type-erased thunk and call to_js() lazily at drain time, on the draining
 * thread. For the no-callback case the whole machinery is compiled out.
 */

#include <avnd/binding/wasm/controls.hpp>
#include <avnd/concepts/callback.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/callbacks_adapter.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <boost/mp11.hpp>

#include <functional>
#include <string>
#include <tuple>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/val.h>
#endif

namespace wasm
{

#if defined(__EMSCRIPTEN__)

// One queued callback emission: which callback fired, and a thunk that builds
// the JS argument array from the captured native args at drain time.
struct callback_event
{
  int index{};
  std::function<emscripten::val()> make_args; // -> JS array of converted args
};

// Per-instance queue of emitted callback events. Single-threaded-per-instance
// assumption: no locking.
struct callback_queue
{
  std::vector<callback_event> events;

  // Capture an emission; args are copied by value, to_js() deferred to drain().
  template <typename... Args>
  void emit(int index, Args... args)
  {
    auto thunk = [args...]() -> emscripten::val {
      emscripten::val arr = emscripten::val::array();
      int i = 0;
      ((arr.set(i++, wasm::to_js(args))), ...);
      return arr;
    };
    events.push_back(callback_event{index, std::move(thunk)});
  }

  // Build the JS-facing array of {index, name, args:[...]} emitted since the
  // last drain, then clear. `names` maps callback index -> name.
  template <typename T>
  emscripten::val drain(const std::vector<std::string>& names)
  {
    emscripten::val out = emscripten::val::array();
    int n = 0;
    for(auto& ev : events)
    {
      emscripten::val o = emscripten::val::object();
      o.set("index", emscripten::val(ev.index));
      if(ev.index >= 0 && ev.index < (int)names.size())
        o.set("name", emscripten::val(names[ev.index]));
      else
        o.set("name", emscripten::val(std::string{}));
      o.set("args", ev.make_args());
      out.set(n++, o);
    }
    events.clear();
    return out;
  }
};

#endif // __EMSCRIPTEN__

template <typename T>
constexpr int callback_count() noexcept
{
  using outputs_t = typename avnd::outputs_type<T>::type;
  return (int)avnd::callback_introspection<outputs_t>::size;
}

// Visit callback #i (0-based, in callback-predicate order) with its field.
template <typename T, typename F>
inline void for_callback_nth(avnd::effect_container<T>& obj, int i, F&& f)
{
  using outputs_t = typename avnd::outputs_type<T>::type;
  if constexpr(avnd::callback_introspection<outputs_t>::size > 0)
  {
    int idx = 0;
    avnd::callback_introspection<outputs_t>::for_all(
        avnd::get_outputs(obj), [&]<typename Field>(Field& field) {
      if(idx++ == i)
        f.template operator()<Field>(field);
    });
  }
}

// The call signature `R(Args...)` of a callback Field, with the leading void*
// context stripped for view callbacks.
template <typename Field>
struct callback_signature
{
  using type = std::decay_t<decltype(Field{}.call)>;
};
template <avnd::view_callback Field>
struct callback_signature<Field>
{
  using type = avnd::view_callback_message_type<Field>;
};
template <typename Field>
using callback_signature_t = typename callback_signature<Field>::type;

// Number of JS-visible arguments a callback Field takes.
template <typename Field>
constexpr int callback_js_arg_count() noexcept
{
  using refl = avnd::function_reflection_t<callback_signature_t<Field>>;
  return (int)refl::count;
}

// Names of all callback outputs in callback-predicate order. Works whether
// `outputs` is a type or a value.
template <typename T>
inline std::vector<std::string> callback_names(avnd::effect_container<T>& obj)
{
  std::vector<std::string> names;
  using outputs_t = typename avnd::outputs_type<T>::type;
  if constexpr(avnd::callback_introspection<outputs_t>::size > 0)
  {
    names.reserve(avnd::callback_introspection<outputs_t>::size);
    avnd::callback_introspection<outputs_t>::for_all(
        avnd::get_outputs(obj), [&]<typename Field>(Field& /*field*/) {
      names.push_back(std::string{avnd::get_name<Field>()});
    });
  }
  return names;
}

} // namespace wasm
