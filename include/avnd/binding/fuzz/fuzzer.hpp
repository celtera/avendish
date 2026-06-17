#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Fuzz-testing harness.
 *
 * Counterpart of the `dump` harness: where `dump` walks a processor's
 * introspection to *print* every port, the fuzz harness walks the same
 * introspection to *populate* every input from a coverage-guided fuzzer's byte
 * stream and then *run* the node, so that ASan/UBSan/libFuzzer can surface
 * crashes, OOB accesses, integer UB, leaks, etc.
 *
 * Engine: libFuzzer (`-fsanitize=fuzzer`). The translation unit produced by
 * prototype.cpp.in defines `LLVMFuzzerTestOneInput`, which calls fuzz::run<T>().
 *
 * One libFuzzer input drives a whole session: pick a configuration, then a
 * fuzzer-chosen number of iterations, each of which re-fuzzes the inputs and
 * runs the node once (exercising state carried across calls).
 */

#include <avnd/concepts/audio_processor.hpp>
#include <avnd/concepts/midi_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/concepts/processor.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/midi.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/callbacks_adapter.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_storage.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/prepare.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/process_execution.hpp>
#include <avnd/wrappers/ranges.hpp>

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>

#include <fuzzer/FuzzedDataProvider.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace fuzz_harness
{
/** A logger that does nothing: the fuzz loop must stay free of I/O. */
struct silent_logger
{
  template <typename... Args> constexpr void trace(Args&&...) const noexcept { }
  template <typename... Args> constexpr void info(Args&&...) const noexcept { }
  template <typename... Args> constexpr void debug(Args&&...) const noexcept { }
  template <typename... Args> constexpr void warn(Args&&...) const noexcept { }
  template <typename... Args> constexpr void error(Args&&...) const noexcept { }
  template <typename... Args> constexpr void critical(Args&&...) const noexcept { }
};

// ---------------------------------------------------------------------------
// Callback / std::function wiring
//
// A node may invoke a host-provided std::function (a callback, a buffer upload,
// a channel-resize request...). If we leave it empty, calling it terminates. So
// before running we install no-op handlers, the same way the ossia/godot
// bindings reach these members -- by visiting each *port* and acting on its
// known structure, NOT by recursing through arbitrary aggregates:
//   1. callback output ports (halp::callback<>) -- via wrap_callbacks, which
//      uses the stored/view callback introspection visitors;
//   2. the per-port callables a node may call from prepare()/operator():
//        - port.buffer.upload     (buffer / geometry output ports)
//        - port.request_channels  (variable-channel audio busses)
//      TODO: dynamic-port request_port_resize, curve and custom-widget hooks
//      are not wired yet (no registered example exercises them).
// ---------------------------------------------------------------------------
// Visit each relevant port *kind* via its filtered introspection (the same
// per-kind visitors the WASM/godot/ossia bindings use) and wire that kind's
// callable. Guarded by `::size > 0`, so objects without a kind compile it out
// entirely -- no catch-all iteration over unrelated ports.
template <typename T>
void wire_ports(avnd::effect_container<T>& effect)
{
  // Output buffer / geometry ports push their data through buffer.upload.
  auto wire_buffer = [](auto& p) {
    if constexpr(requires { p.buffer.upload; })
      if(!p.buffer.upload)
        p.buffer.upload = [](const char*, std::int64_t, std::int64_t) {};
  };
  if constexpr(avnd::buffer_output_introspection<T>::size > 0)
    avnd::buffer_output_introspection<T>::for_all(avnd::get_outputs(effect), wire_buffer);
  if constexpr(avnd::buffer_input_introspection<T>::size > 0)
    avnd::buffer_input_introspection<T>::for_all(avnd::get_inputs(effect), wire_buffer);

  // Variable-channel audio busses ask the host for channels via request_channels.
  auto wire_channels = [](auto& p) {
    if constexpr(requires { p.request_channels; })
      if(!p.request_channels)
        p.request_channels = [](int) {};
  };
  if constexpr(avnd::audio_bus_input_introspection<T>::size > 0)
    avnd::audio_bus_input_introspection<T>::for_all(avnd::get_inputs(effect), wire_channels);
  if constexpr(avnd::audio_bus_output_introspection<T>::size > 0)
    avnd::audio_bus_output_introspection<T>::for_all(avnd::get_outputs(effect), wire_channels);
}

// Callback output ports (halp::callback<>): install no-op handlers via the
// stored/view callback introspection visitors that callback_storage drives.
template <typename T>
void install_callbacks(avnd::effect_container<T>& effect, avnd::callback_storage<T>& storage)
{
  using outputs_t = typename avnd::outputs_type<T>::type;
  if constexpr(avnd::callback_introspection<outputs_t>::size > 0)
  {
    storage.wrap_callbacks(
        effect,
        []<typename Field, template <typename...> typename L, typename Ret,
           typename... Args, std::size_t Idx>(
            Field&, L<Ret, Args...>, avnd::num<Idx>) {
      if constexpr(std::is_void_v<Ret>)
        return [](Args...) {};
      else
        return [](Args...) -> Ret { return Ret{}; };
    });
  }
}

// Bound on fuzzer-controlled sizes: keeps each run fast and the verifier of
// "did we run out of bytes" cheap. Tunable; not security-sensitive.
inline constexpr int max_channels = 8;
inline constexpr int max_frames = 1024;
inline constexpr int max_iterations = 32;
inline constexpr int max_string_len = 64;
inline constexpr int max_midi_msgs = 8;
inline constexpr int max_texture_dim = 64;

/**
 * Robustness knob: how often (per mille) we leave the "realistic, in-range"
 * regime and feed an out-of-range / special value (NaN, ±inf, type extremes,
 * boundaries). Real hosts clamp controls to their range, so the bulk of fuzzing
 * stays in-range to explore valid logic deeply; this fraction probes what
 * happens when an out-of-range value sneaks in anyway. Tunable at runtime via
 * $AVND_FUZZ_OOR_PERMILLE (0 disables, 1000 = always out-of-range). Default 10%.
 */
inline int oor_permille() noexcept
{
  static const int v = [] {
    if(const char* e = std::getenv("AVND_FUZZ_OOR_PERMILLE"))
    {
      const int x = std::atoi(e);
      return x < 0 ? 0 : (x > 1000 ? 1000 : x);
    }
    return 100;
  }();
  return v;
}

inline bool roll_oor(FuzzedDataProvider& fdp) noexcept
{
  const int p = oor_permille();
  return p > 0 && fdp.ConsumeIntegralInRange<int>(1, 1000) <= p;
}

/** An adversarial value for an arithmetic type: extremes + (for fp) NaN/±inf. */
template <typename V>
V special_value(FuzzedDataProvider& fdp) noexcept
{
  using L = std::numeric_limits<V>;
  if constexpr(std::is_floating_point_v<V>)
  {
    const V vals[] = {
        V(0),           V(-0.0),         V(1),         V(-1),
        L::quiet_NaN(), L::infinity(),   -L::infinity(),
        L::lowest(),    (L::max)(),      L::denorm_min()};
    return vals[fdp.ConsumeIntegralInRange<std::size_t>(0, std::size(vals) - 1)];
  }
  else
  {
    const V vals[] = {V(0), V(1), V(-1), (L::min)(), (L::max)()};
    return vals[fdp.ConsumeIntegralInRange<std::size_t>(0, std::size(vals) - 1)];
  }
}

/** Produce a value of an arithmetic / bool / enum scalar type from the stream. */
template <typename V>
V fuzz_scalar(FuzzedDataProvider& fdp)
{
  if constexpr(std::is_same_v<V, bool>)
    return fdp.ConsumeBool();
  else if constexpr(std::is_enum_v<V>)
    return static_cast<V>(fdp.ConsumeIntegral<std::underlying_type_t<V>>());
  else if constexpr(std::is_floating_point_v<V>)
    return roll_oor(fdp) ? special_value<V>(fdp) : fdp.ConsumeFloatingPoint<V>();
  else if constexpr(std::is_integral_v<V>)
    return roll_oor(fdp) ? special_value<V>(fdp) : fdp.ConsumeIntegral<V>();
  else
    return V{};
}

/** A value within [lo,hi], but `oor_permille` of the time deliberately outside. */
template <typename V>
V fuzz_ranged(FuzzedDataProvider& fdp, V lo, V hi)
{
  if(roll_oor(fdp))
  {
    // Half the time a wild special (NaN/inf/extreme); half an "expanded range"
    // value just outside [lo,hi] (boundary / off-by-range bugs).
    if(fdp.ConsumeBool())
      return special_value<V>(fdp);
    // Otherwise a value just outside [lo,hi] (boundary / off-by-range bugs).
    // Computed with saturation so the harness never overflows for ranges near
    // the type's limits (which would be harness-side UB, not a node finding).
    if constexpr(std::is_floating_point_v<V>)
    {
      const V span = (hi > lo) ? (hi - lo) : V(1);
      V elo = lo - span, ehi = hi + span;
      if(!std::isfinite(elo))
        elo = lo;
      if(!std::isfinite(ehi))
        ehi = hi;
      return fdp.ConsumeFloatingPointInRange<V>(elo, ehi);
    }
    else
    {
      using S = std::int64_t;
      constexpr S smin = std::numeric_limits<S>::min();
      constexpr S smax = std::numeric_limits<S>::max();
      const auto l = static_cast<S>(lo);
      const auto h = static_cast<S>(hi);
      constexpr S margin = 256; // bounded probe past the edges; extremes are special_value's job
      const S elo = (l < smin + margin) ? smin : S(l - margin);
      const S ehi = (h > smax - margin) ? smax : S(h + margin);
      return static_cast<V>(fdp.ConsumeIntegralInRange<S>(elo, ehi));
    }
  }
  if constexpr(std::is_floating_point_v<V>)
    return fdp.ConsumeFloatingPointInRange<V>(lo, hi);
  else
    return static_cast<V>(fdp.ConsumeIntegralInRange<std::int64_t>(
        static_cast<std::int64_t>(lo), static_cast<std::int64_t>(hi)));
}

/** Produce a value of one message-argument type (decayed). */
template <typename A>
auto fuzz_arg(FuzzedDataProvider& fdp)
{
  using V = std::remove_cvref_t<A>;
  if constexpr(std::is_same_v<V, std::string>)
    return fdp.ConsumeRandomLengthString(max_string_len);
  else if constexpr(std::is_same_v<V, const char*> || std::is_same_v<V, char*>)
  {
    // Back the C string with thread-local storage so the pointer stays valid
    // for the call, and never hand the node a null (would crash printf("%s")).
    static thread_local std::string s;
    s = fdp.ConsumeRandomLengthString(max_string_len);
    return const_cast<V>(s.c_str());
  }
  else if constexpr(
      std::is_arithmetic_v<V> || std::is_enum_v<V> || std::is_same_v<V, bool>)
    return fuzz_scalar<V>(fdp);
  else
    return V{}; // vectors, structs, ...: default-constructed (still exercises the call)
}

/** Assign fuzzed values to every parameter input, then fire value-port updates. */
template <typename T>
void fuzz_parameters(avnd::effect_container<T>& effect, FuzzedDataProvider& fdp)
{
  if constexpr(avnd::parameter_input_introspection<T>::size > 0)
  {
    // NB: re-fetch get_inputs() for each traversal -- for value-input containers
    // it returns a move-only, single-pass coroutine iterator, so the same handle
    // cannot be iterated twice.
    // Pass 1: set the values. Respect each control's declared range: real hosts
    // (Max/VST/...) clamp controls to [min,max], so feeding out-of-range values
    // is unrealistic and merely manufactures false positives (e.g. a gain used
    // as a loop bound, fuzzed to 2^31, becomes a multi-billion-iteration "hang").
    avnd::parameter_input_introspection<T>::for_all(
        avnd::get_inputs(effect), [&]<typename F>(F& field) {
      using type = std::remove_cvref_t<F>;
      if constexpr(avnd::string_parameter<type>)
      {
        field.value = fdp.ConsumeRandomLengthString(max_string_len);
      }
      else if constexpr(avnd::enum_parameter<type>)
      {
        using V = std::remove_cvref_t<decltype(field.value)>;
        constexpr int n = avnd::get_enum_choices_count<type>();
        if constexpr(n > 0)
        {
          // Mostly a valid choice; occasionally an out-of-range index to probe
          // nodes that index arrays/switch on the enum without bounds-checking.
          if(roll_oor(fdp))
            field.value = static_cast<V>(fuzz_scalar<std::underlying_type_t<V>>(fdp));
          else
            field.value = static_cast<V>(fdp.ConsumeIntegralInRange<int>(0, n - 1));
        }
        else
          field.value = fuzz_scalar<V>(fdp);
      }
      else if constexpr(
          avnd::parameter_with_minmax_range_ignore_init<type>
          && requires { field.value; })
      {
        using V = std::remove_cvref_t<decltype(field.value)>;
        constexpr auto r = avnd::get_range<type>();
        if constexpr(std::is_floating_point_v<V> || std::is_integral_v<V>)
          field.value = fuzz_ranged<V>(fdp, static_cast<V>(r.min), static_cast<V>(r.max));
        else
          field.value = fuzz_scalar<V>(fdp);
      }
      else if constexpr(requires { field.value; })
      {
        field.value = fuzz_scalar<std::remove_cvref_t<decltype(field.value)>>(fdp);
      }
    });

    // Pass 2: Max/Pd-style value ports notify the object through update(self).
    for(auto& self : effect.effects())
    {
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(effect), [&](auto& field) {
        if constexpr(requires { field.update(self); })
          field.update(self);
      });
    }
  }
}

// ---------------------------------------------------------------------------
// MIDI 1.0 output validity oracle
//
// avendish processors speak MIDI 1.0 byte messages (UMP is only a host-side
// conversion in the ossia binding, so there are no native UMP ports to check).
// A node that emits a malformed MIDI message is a bug; we validate each emitted
// message and abort (so libFuzzer records it) on a clear violation.
// ---------------------------------------------------------------------------
inline bool valid_midi1(const std::uint8_t* b, std::size_t n) noexcept
{
  if(n == 0)
    return false;
  const std::uint8_t s = b[0];
  if(s < 0x80) // a message must start with a status byte
    return false;
  if(s == 0xF0) // SysEx: 7-bit payload terminated by 0xF7
  {
    if(n < 2 || b[n - 1] != 0xF7)
      return false;
    for(std::size_t i = 1; i + 1 < n; i++)
      if(b[i] > 0x7F)
        return false;
    return true;
  }
  std::size_t data = 0;
  if(s >= 0xF8)                                data = 0; // system real-time
  else if(s == 0xF1 || s == 0xF3)              data = 1;
  else if(s == 0xF2)                           data = 2;
  else if(s == 0xF6)                           data = 0; // tune request
  else if(s == 0xF4 || s == 0xF5 || s == 0xF7) return false; // undefined / lone EOX
  else // channel-voice 0x80..0xEF
  {
    const std::uint8_t hi = s & 0xF0;
    data = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
  }
  if(n != 1 + data)
    return false;
  for(std::size_t i = 1; i < n; i++) // data bytes are 7-bit
    if(b[i] > 0x7F)
      return false;
  return true;
}

// Total length implied by a status byte, for fixed-size array messages that
// carry no explicit size. 0 means variable/unknown (sysex/undefined).
inline std::size_t midi1_total_len(std::uint8_t s) noexcept
{
  if(s < 0x80) return 0;
  if(s >= 0xF8) return 1;
  if(s == 0xF1 || s == 0xF3) return 2;
  if(s == 0xF2) return 3;
  if(s == 0xF6) return 1;
  if(s >= 0xF0) return 0;
  const std::uint8_t hi = s & 0xF0;
  return (hi == 0xC0 || hi == 0xD0) ? 2 : 3;
}

[[noreturn]] inline void report_invalid_midi(const std::uint8_t* b, std::size_t n)
{
  std::fprintf(stderr, "fuzz: node emitted an invalid MIDI-1 message:");
  for(std::size_t i = 0; i < n; i++)
    std::fprintf(stderr, " %02X", (unsigned)b[i]);
  std::fprintf(stderr, "\n");
  std::abort();
}

template <typename Msg>
void check_midi_message(const Msg& m)
{
  if constexpr(requires { m.bytes.data(); m.bytes.size(); })
  {
    if(!valid_midi1(m.bytes.data(), m.bytes.size()))
      report_invalid_midi(m.bytes.data(), m.bytes.size());
  }
  else // fixed-size byte array: length is implied by the status byte
  {
    const std::uint8_t* b = &m.bytes[0];
    const std::size_t n = midi1_total_len(b[0]); // 0 = variable/unknown
    if(n == 0 || n > sizeof(m.bytes) || !valid_midi1(b, n))
      // never read past the array when reporting
      report_invalid_midi(b, (n == 0 || n > sizeof(m.bytes)) ? sizeof(m.bytes) : n);
  }
}

/** Validate every message a node emitted on its MIDI output ports. */
template <typename T>
void validate_midi_outputs(avnd::effect_container<T>& effect)
{
  if constexpr(avnd::midi_output_introspection<T>::size > 0)
  {
    auto&& outs = avnd::get_outputs(effect);
    avnd::midi_output_introspection<T>::for_all(outs, [&]<typename P>(P& port) {
      if constexpr(avnd::dynamic_container_midi_port<P>)
        for(auto& m : port.midi_messages)
          check_midi_message(m);
      else if constexpr(avnd::raw_container_midi_port<P>)
        for(std::size_t i = 0; i < std::size_t(port.size); i++)
          check_midi_message(port.midi_messages[i]);
    });
  }
}

/** Feed MIDI input ports. Returns true if any *deliberately invalid* message
 * was injected this round -- the caller then skips output validation, since a
 * passthrough echoing our garbage is not a node bug. */
template <typename T>
bool fuzz_midi(avnd::effect_container<T>& effect, FuzzedDataProvider& fdp)
{
  bool tainted = false;
  if constexpr(avnd::midi_input_introspection<T>::size > 0)
  {
    auto&& ins = avnd::get_inputs(effect);
    avnd::midi_input_introspection<T>::for_all(ins, [&]<typename P>(P& port) {
      if constexpr(avnd::dynamic_container_midi_port<P>)
      {
        const int n = fdp.ConsumeIntegralInRange<int>(0, max_midi_msgs);
        for(int i = 0; i < n; i++)
        {
          if(roll_oor(fdp))
          {
            // Robustness: feed a raw, possibly-malformed message. Taint the run.
            tainted = true;
            port.midi_messages.push_back(
                {.bytes
                 = {fdp.ConsumeIntegral<unsigned char>(),
                    fdp.ConsumeIntegral<unsigned char>(),
                    fdp.ConsumeIntegral<unsigned char>()},
                 .timestamp = 0});
          }
          else
          {
            // A well-formed MIDI-1 channel-voice message.
            static constexpr std::uint8_t kinds[]
                = {0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0};
            const std::uint8_t hi = kinds[fdp.ConsumeIntegralInRange<int>(0, 6)];
            const auto st = std::uint8_t(hi | fdp.ConsumeIntegralInRange<unsigned>(0, 15));
            const auto d0 = std::uint8_t(fdp.ConsumeIntegral<std::uint8_t>() & 0x7F);
            if(hi == 0xC0 || hi == 0xD0)
              port.midi_messages.push_back({.bytes = {st, d0}, .timestamp = 0});
            else
            {
              const auto d1 = std::uint8_t(fdp.ConsumeIntegral<std::uint8_t>() & 0x7F);
              port.midi_messages.push_back({.bytes = {st, d0, d1}, .timestamp = 0});
            }
          }
        }
      }
    });
  }
  return tainted;
}

// Invoke message M on object `self` with the fuzzed `args`, covering every
// reflectable kind via message_get_func<M>(): function-object (operator()),
// stateless member-fn-pointer of T, free function, lambda and std::function.
// Mirrors the Pd/Max binding dispatch. `with_self` prepends the object.
template <typename M, typename Self, typename Tuple, std::size_t... I>
void dispatch_message(Self& self, Tuple& args, std::index_sequence<I...>, bool with_self)
{
  auto f = avnd::message_get_func<M>();
  using FT = std::decay_t<decltype(f)>;
  if(with_self)
  {
    if constexpr(std::is_member_function_pointer_v<FT>)
    {
      if constexpr(
          std::is_default_constructible_v<M>
          && requires(M m) { m(self, std::get<I>(args)...); })
        M{}(self, std::get<I>(args)...);          // functor taking self
      else if constexpr(requires { (self.*f)(self, std::get<I>(args)...); })
        (self.*f)(self, std::get<I>(args)...);
    }
    else if constexpr(requires { f(self, std::get<I>(args)...); })
      f(self, std::get<I>(args)...);              // free/lambda/std::function taking self
  }
  else
  {
    if constexpr(std::is_member_function_pointer_v<FT>)
    {
      if constexpr(
          std::is_default_constructible_v<M>
          && requires(M m) { m(std::get<I>(args)...); })
        M{}(std::get<I>(args)...);                // functor
      else if constexpr(requires { (self.*f)(std::get<I>(args)...); })
        (self.*f)(std::get<I>(args)...);          // stateless member fn of T
    }
    else if constexpr(requires { f(std::get<I>(args)...); })
      f(std::get<I>(args)...);                    // free/lambda/std::function
  }
}

// Build fuzzed values for a message's (self-stripped) argument list and invoke.
template <typename T, typename M, typename... As>
void invoke_message(
    avnd::effect_container<T>& effect, FuzzedDataProvider& fdp, boost::mp11::mp_list<As...>)
{
  using list = boost::mp11::mp_list<As...>;
  // NB: don't let `&&` instantiate mp_first on an empty argument list.
  constexpr bool takes_self = [] {
    if constexpr(sizeof...(As) >= 1)
      return std::is_same_v<std::remove_cvref_t<boost::mp11::mp_first<list>>, T>;
    else
      return false;
  }();

  if constexpr(takes_self)
  {
    using rest = boost::mp11::mp_rest<list>;
    [&]<typename... Bs>(boost::mp11::mp_list<Bs...>) {
      std::tuple<std::remove_cvref_t<Bs>...> args{fuzz_arg<Bs>(fdp)...};
      for(auto& self : effect.effects())
        dispatch_message<M>(self, args, std::index_sequence_for<Bs...>{}, true);
    }(rest{});
  }
  else
  {
    std::tuple<std::remove_cvref_t<As>...> args{fuzz_arg<As>(fdp)...};
    for(auto& self : effect.effects())
      dispatch_message<M>(self, args, std::index_sequence_for<As...>{}, false);
  }
}

/** Call every message handler (all reflectable kinds + variadic), fuzzed. */
template <typename T>
void fuzz_messages(avnd::effect_container<T>& effect, FuzzedDataProvider& fdp)
{
  // NB: introspect on the processor type T, not on the container -- and iterate
  // get_messages(effect) directly. (avnd::for_all_messages deduces its type
  // from its argument, so passing the container makes its own size-guard 0.)
  if constexpr(avnd::messages_introspection<T>::size > 0)
  {
    avnd::for_each_field_ref(avnd::get_messages(effect), [&]<typename M>(M& /*field*/) {
      if constexpr(avnd::reflectable_message<M>)
      {
        using refl = avnd::message_reflection<M>;
        invoke_message<T, M>(effect, fdp, typename refl::arguments{});
      }
      // NB: variadic / "any-message" handlers (e.g. INPUT_RANGE auto) are not
      // invoked: they expect a host-specific variant range, and instantiating
      // their body with a plain container fails to compile (ADL visit, etc.).
    });
  }
}

/** Feed each *CPU* texture input port a fuzzed image so filters actually run
 * (most guard on `bytes == nullptr` / `changed` and bail otherwise). One
 * persistent backing buffer per input texture, re-filled each iteration.
 * `bufs` must be sized to cpu_texture_input_introspection<T>::size.
 * Only CPU textures expose `.texture.bytes`; gpu/sampler/image inputs are
 * handle-based and have no host-visible pixels to fuzz. */
template <typename T>
void fuzz_texture_inputs(
    avnd::effect_container<T>& effect, FuzzedDataProvider& fdp,
    std::vector<std::vector<unsigned char>>& bufs)
{
  auto&& ins = avnd::get_inputs(effect);
  avnd::cpu_texture_input_introspection<T>::for_all_n(
      ins, [&]<auto Idx, typename P>(P& port, avnd::predicate_index<Idx>) {
    auto& tex = port.texture;
    using tex_t = std::decay_t<decltype(tex)>;
    // bytes_per_pixel is a static constexpr for fixed formats, a member fn for
    // custom formats (mirrors halp's texture_output::create).
    std::size_t bpp;
    if constexpr(requires { tex.bytes_per_pixel(); })
      bpp = tex.bytes_per_pixel();
    else
      bpp = tex_t::bytes_per_pixel;

    const int w = fdp.ConsumeIntegralInRange<int>(1, max_texture_dim);
    const int h = fdp.ConsumeIntegralInRange<int>(1, max_texture_dim);

    auto& buf = bufs[Idx];
    buf.resize(std::size_t(w) * std::size_t(h) * bpp);
    fdp.ConsumeData(buf.data(), buf.size());

    // The vector's storage comes from ::operator new, aligned to
    // __STDCPP_DEFAULT_NEW_ALIGNMENT__ (>= alignof of any texel type), so the
    // reinterpret_cast to the texture's (possibly float) pointer is well-aligned.
    tex.width = w;
    tex.height = h;
    tex.bytes = reinterpret_cast<decltype(tex.bytes)>(buf.data());
    tex.changed = true;
  });
}

/** Run a non-audio object once (message / data processor). */
template <typename T>
void run_object(avnd::effect_container<T>& effect)
{
  for(auto& impl : effect.effects())
  {
    if constexpr(requires { impl(); })
      impl();
  }
}

/** One libFuzzer input. Returns 0 (libFuzzer ignores non-zero on this path). */
template <typename T>
int run(const std::uint8_t* data, std::size_t size)
{
  FuzzedDataProvider fdp(data, size);

  avnd::effect_container<T> effect;
  avnd::process_adapter<T> processor;
  avnd::midi_storage<T> midi;
  avnd::control_storage<T> controls;
  avnd::callback_storage<T> callbacks;

  avnd::init_controls(effect);

  constexpr bool is_audio
      = avnd::monophonic_audio_processor<T> || avnd::polyphonic_audio_processor<T>;

  const int frames = fdp.ConsumeIntegralInRange<int>(1, max_frames);

  // --- Per-node storage allocated once, outside the iteration loop ---
  std::vector<std::vector<float>> in_storage, out_storage;
  std::vector<float*> in_ptrs, out_ptrs;
  int in_ch = 0, out_ch = 0;

  if constexpr(is_audio)
  {
    // Use a single requested count for both busses: effect processors index the
    // output bus by the *input* channel count, so feeding mismatched in/out
    // counts is a harness misconfiguration, not a node bug. Nodes with fixed,
    // asymmetric channel counts override the request via input/output_channels<T>().
    const int req_ch = fdp.ConsumeIntegralInRange<int>(1, max_channels);
    in_ch = avnd::input_channels<T>(req_ch);
    out_ch = avnd::output_channels<T>(req_ch);
    if(in_ch < 0)
      in_ch = req_ch;
    if(out_ch < 0)
      out_ch = req_ch;

    effect.init_channels(in_ch, out_ch);

    avnd::process_setup setup{
        .input_channels = in_ch,
        .output_channels = out_ch,
        .frames_per_buffer = frames,
        .rate = 48000.};

    processor.allocate_buffers(setup, float{});
    midi.reserve_space(effect, frames);
    controls.reserve_space(effect, frames);
    avnd::prepare(effect, setup);

    in_storage.assign(in_ch, std::vector<float>(frames, 0.f));
    out_storage.assign(out_ch, std::vector<float>(frames, 0.f));
    in_ptrs.resize(in_ch);
    out_ptrs.resize(out_ch);
    for(int c = 0; c < in_ch; c++)
      in_ptrs[c] = in_storage[c].data();
    for(int c = 0; c < out_ch; c++)
      out_ptrs[c] = out_storage[c].data();
  }
  else
  {
    // Reserve MIDI storage whenever the node has MIDI ports, independent of
    // prepare(): raw-container ports need their `size`/pointer initialized
    // before we read outputs, and prepare() is optional for non-audio nodes.
    if constexpr(
        avnd::midi_input_introspection<T>::size > 0
        || avnd::midi_output_introspection<T>::size > 0)
      midi.reserve_space(effect, frames);

    if constexpr(avnd::can_prepare<T>)
    {
      avnd::process_setup setup{
          .input_channels = 0,
          .output_channels = 0,
          .frames_per_buffer = frames,
          .rate = 48000.};
      controls.reserve_space(effect, frames);
      avnd::prepare(effect, setup);
    }
  }

  // Wire every callable the node might invoke, so an unwired callback can never
  // std::terminate(): proper callback ports first, then any remaining raw
  // std::function reachable in the ports.
  install_callbacks(effect, callbacks);
  wire_ports(effect);

  // Persistent backing storage for fuzzed input textures (one buffer per port).
  std::vector<std::vector<unsigned char>> tex_in_bufs(
      avnd::cpu_texture_input_introspection<T>::size);

  const int iterations = fdp.ConsumeIntegralInRange<int>(1, max_iterations);
  for(int it = 0; it < iterations && fdp.remaining_bytes() > 0; ++it)
  {
    fuzz_parameters(effect, fdp);
    const bool midi_tainted = fuzz_midi(effect, fdp);
    fuzz_messages(effect, fdp);
    if constexpr(avnd::cpu_texture_input_introspection<T>::size > 0)
      fuzz_texture_inputs(effect, fdp, tex_in_bufs);

    if constexpr(is_audio)
    {
      for(int c = 0; c < in_ch; c++)
        for(int s = 0; s < frames; s++)
          in_storage[c][s] = fdp.ConsumeFloatingPoint<float>();

      // Occasionally splatter a special sample (NaN/inf/extreme) into each
      // channel to exercise denormal / NaN-propagation handling.
      if(roll_oor(fdp))
        for(int c = 0; c < in_ch; c++)
          in_storage[c][fdp.ConsumeIntegralInRange<int>(0, frames - 1)]
              = special_value<float>(fdp);

      processor.process(
          effect, avnd::span<float*>{in_ptrs.data(), std::size_t(in_ch)},
          avnd::span<float*>{out_ptrs.data(), std::size_t(out_ch)}, frames);

      if(!midi_tainted)
        validate_midi_outputs(effect);
      midi.clear_inputs(effect);
      midi.clear_outputs(effect);
      controls.clear_inputs(effect);
      controls.clear_outputs(effect);
    }
    else
    {
      run_object(effect);
      if(!midi_tainted)
        validate_midi_outputs(effect);
      midi.clear_inputs(effect);
      midi.clear_outputs(effect);
    }
  }

  return 0;
}
}
