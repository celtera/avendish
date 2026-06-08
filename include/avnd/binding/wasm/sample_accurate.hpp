#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Sample-accurate control scheduling for the WASM backend. Three avnd storage
 * flavours exist (linear/span/dynamic, see concepts/parameter.hpp);
 * schedule_control indexes ports the same way as setParameter() and falls back
 * to an immediate set_field() for non-sample-accurate controls.
 */

#include <avnd/binding/wasm/controls.hpp>
#include <avnd/common/tuple.hpp>
#include <avnd/concepts/control.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_storage.hpp>
#include <avnd/wrappers/effect_container.hpp>

#include <algorithm>

#if defined(__EMSCRIPTEN__)
#include <emscripten/val.h>
#endif

namespace wasm
{

#if defined(__EMSCRIPTEN__)

// Append one timestamped value into a span sample-accurate port. The span does
// not own storage, so we find the matching backing vector in control_storage
// (by pointer identity of the port) and re-point the span at it.
template <typename T, typename Field, typename Storage>
inline void schedule_span(
    avnd::effect_container<T>& obj, Field& ctl, Storage& storage, int sampleOffset,
    const emscripten::val& v, int buffer_size)
{
  if(sampleOffset < 0 || sampleOffset >= buffer_size)
    return;

  using tv_t = typename std::decay_t<decltype(Field::values)>::value_type;
  using elem_t = std::decay_t<decltype(std::declval<tv_t>().value)>;
  elem_t tmp{};
  wasm::from_js(tmp, v);

  // PredIdx over the span-timed introspection == the storage tuple index.
  using span_in = avnd::span_timed_parameter_input_introspection<T>;
  span_in::for_all_n(
      avnd::get_inputs(obj),
      [&]<auto SIdx, typename SField>(SField& sctl, avnd::predicate_index<SIdx>) {
    if constexpr(std::is_same_v<SField, Field>)
    {
      if((void*)&sctl != (void*)&ctl)
        return;
      auto& buf = tpl::get<SIdx>(storage.span_timed_inputs);
      buf.push_back(tv_t{.value = tmp, .frame = sampleOffset});
      ctl.values = {buf.data(), buf.size()};
    }
  });
  ctl.value = tmp;
}

// Schedule a timestamped value into input control #paramIndex.
template <typename T, typename Storage>
inline void schedule_control(
    avnd::effect_container<T>& obj, Storage& storage, int paramIndex,
    int sampleOffset, const emscripten::val& v, int buffer_size)
{
  using param_in = avnd::parameter_input_introspection<T>;
  if constexpr(param_in::size > 0)
  {
    // PredIdx over parameter inputs == the paramIndex used by setParameter().
    param_in::for_all_n(
        avnd::get_inputs(obj),
        [&]<auto PredIdx, typename Field>(
            Field& ctl, avnd::predicate_index<PredIdx>) {
      if((int)PredIdx != paramIndex)
        return;

      if constexpr(avnd::linear_sample_accurate_parameter_port<Field>)
      {
        if(sampleOffset < 0 || sampleOffset >= buffer_size)
          return;
        using opt_t = std::remove_pointer_t<std::decay_t<decltype(Field::values)>>;
        using val_t = std::decay_t<decltype(*std::declval<opt_t&>())>;
        val_t tmp{};
        wasm::from_js(tmp, v);
        ctl.values[sampleOffset] = tmp;
        ctl.value = tmp;
      }
      else if constexpr(avnd::dynamic_sample_accurate_parameter_port<Field>)
      {
        if(sampleOffset < 0 || sampleOffset >= buffer_size)
          return;
        using mapped_t = std::decay_t<
            typename std::decay_t<decltype(Field::values)>::mapped_type>;
        mapped_t tmp{};
        wasm::from_js(tmp, v);
        ctl.values[sampleOffset] = tmp;
        ctl.value = tmp;
      }
      else if constexpr(avnd::span_sample_accurate_parameter_port<Field>)
      {
        schedule_span<T>(obj, ctl, storage, sampleOffset, v, buffer_size);
      }
      else
      {
        // Not sample-accurate: apply immediately, ignoring the timestamp.
        wasm::set_field(ctl, obj.effect, v);
      }
    });
  }
}

// Drain sample-accurate output control ports to JS.
template <typename T, typename Storage>
inline emscripten::val
drain_control_outputs(avnd::effect_container<T>& obj, Storage& /*storage*/)
{
  emscripten::val out = emscripten::val::array();
  using param_out = avnd::parameter_output_introspection<T>;
  if constexpr(param_out::size > 0)
  {
    int outArrIdx = 0;
    int paramIdx = 0;
    param_out::for_all(avnd::get_outputs(obj), [&]<typename Field>(Field& ctl) {
      const int thisIdx = paramIdx++;
      if constexpr(avnd::sample_accurate_parameter_port<Field>)
      {
        emscripten::val e = emscripten::val::object();
        e.set("index", emscripten::val(thisIdx));
        emscripten::val changes = emscripten::val::array();
        int ci = 0;

        if constexpr(avnd::linear_sample_accurate_parameter_port<Field>)
        {
          // No count available here; linear outputs are surfaced via .value.
        }
        else if constexpr(avnd::span_sample_accurate_parameter_port<Field>)
        {
          for(const auto& tv : ctl.values)
          {
            emscripten::val c = emscripten::val::object();
            c.set("frame", emscripten::val((int)tv.frame));
            c.set("value", wasm::to_js(tv.value));
            changes.set(ci++, std::move(c));
          }
        }
        else if constexpr(avnd::dynamic_sample_accurate_parameter_port<Field>)
        {
          for(const auto& [frame, value] : ctl.values)
          {
            emscripten::val c = emscripten::val::object();
            c.set("frame", emscripten::val((int)frame));
            c.set("value", wasm::to_js(value));
            changes.set(ci++, std::move(c));
          }
        }

        e.set("changes", changes);
        if constexpr(requires { ctl.value; })
          e.set("value", wasm::to_js(ctl.value));
        out.set(outArrIdx++, std::move(e));
      }
    });
  }
  return out;
}

#endif // __EMSCRIPTEN__

} // namespace wasm
