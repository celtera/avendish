#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Control/parameter + output bridge for the WASM backend: the to_js / from_js
 * conversions between native avnd values and emscripten::val, plus the
 * descriptor helpers used by wasm::plugin_module<T>. Recursive over the same
 * type matrix the pd backend handles via t_atom.
 *
 * Iterates parameter ports (anything with a `.value`) rather than control ports,
 * which would also pull in valueless impulse/bang ports.
 * RTTI stays on (Embind needs it); exceptions are off, so nothing here throws.
 */

#include <avnd/binding/wasm/audio_processor.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>

#include <magic_enum/magic_enum.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#if defined(__EMSCRIPTEN__)
#include <emscripten/val.h>
#endif

namespace wasm
{

#if defined(__EMSCRIPTEN__)
using emscripten::val;

// ---------------------------------------------------------------------------
// Native value -> emscripten::val
// ---------------------------------------------------------------------------

inline val to_js(bool v) { return val(v); }
inline val to_js(std::integral auto v) { return val(static_cast<double>(v)); }
inline val to_js(std::floating_point auto v)
{
  return val(static_cast<double>(v));
}

template <typename T>
  requires std::is_enum_v<std::decay_t<T>>
inline val to_js(T v)
{
  // Expose enums by their string name (round-trips through from_js by name).
  return val(std::string{magic_enum::enum_name(v)});
}

inline val to_js(const std::string& v) { return val(v); }
inline val to_js(std::string_view v) { return val(std::string{v}); }
inline val to_js(const char* v) { return val(std::string{v}); }

// Forward declarations for recursion.
template <typename T>
  requires(std::is_aggregate_v<T> && !avnd::iterable_ish<T> && !avnd::tuple_ish<T> && !avnd::bitset_ish<T> && avnd::pfr::tuple_size_v<T> > 0)
val to_js(const T& v);
template <avnd::optional_ish T>
val to_js(const T& v);
template <avnd::pair_ish T>
val to_js(const T& v);
template <avnd::tuple_ish T>
  requires(!avnd::pair_ish<T> && !avnd::iterable_ish<T>)
val to_js(const T& v);
template <avnd::variant_ish T>
val to_js(const T& v);
template <avnd::bitset_ish T>
val to_js(const T& v);
template <avnd::map_ish T>
val to_js(const T& v);
template <typename T>
  requires((avnd::set_ish<T> || avnd::list_ish<T> || avnd::span_ish<T>) && !avnd::string_ish<T> && !avnd::map_ish<T> && !avnd::bitset_ish<T>)
val to_js(const T& v);

// Empty aggregate (impulse / bang): represent as null.
template <typename T>
  requires(std::is_aggregate_v<T> && !avnd::iterable_ish<T> && !avnd::tuple_ish<T> && !avnd::bitset_ish<T> && avnd::pfr::tuple_size_v<T> == 0)
inline val to_js(const T&)
{
  return val::null();
}

template <typename T>
  requires(std::is_aggregate_v<T> && !avnd::iterable_ish<T> && !avnd::tuple_ish<T> && !avnd::bitset_ish<T> && avnd::pfr::tuple_size_v<T> > 0)
inline val to_js(const T& v)
{
  // xy/xyz/rgba: JS object keyed by member name; generic structs: array of fields.
  if constexpr(avnd::xyzw_value<T>)
  {
    val o = val::object();
    o.set("x", to_js(v.x));
    o.set("y", to_js(v.y));
    o.set("z", to_js(v.z));
    o.set("w", to_js(v.w));
    return o;
  }
  else if constexpr(avnd::xyz_value<T>)
  {
    val o = val::object();
    o.set("x", to_js(v.x));
    o.set("y", to_js(v.y));
    o.set("z", to_js(v.z));
    return o;
  }
  else if constexpr(avnd::xy_value<T>)
  {
    val o = val::object();
    o.set("x", to_js(v.x));
    o.set("y", to_js(v.y));
    return o;
  }
  else if constexpr(avnd::rgba_value<T>)
  {
    val o = val::object();
    o.set("r", to_js(v.r));
    o.set("g", to_js(v.g));
    o.set("b", to_js(v.b));
    o.set("a", to_js(v.a));
    return o;
  }
  else if constexpr(avnd::rgb_value<T>)
  {
    val o = val::object();
    o.set("r", to_js(v.r));
    o.set("g", to_js(v.g));
    o.set("b", to_js(v.b));
    return o;
  }
  else
  {
    val arr = val::array();
    int i = 0;
    avnd::for_each_field_ref(
        const_cast<T&>(v), [&]<typename F>(F& field) { arr.set(i++, to_js(field)); });
    return arr;
  }
}

template <avnd::optional_ish T>
inline val to_js(const T& v)
{
  if(v)
    return to_js(*v);
  return val::null();
}

template <avnd::pair_ish T>
inline val to_js(const T& v)
{
  val arr = val::array();
  arr.set(0, to_js(v.first));
  arr.set(1, to_js(v.second));
  return arr;
}

template <avnd::tuple_ish T>
  requires(!avnd::pair_ish<T> && !avnd::iterable_ish<T>)
inline val to_js(const T& v)
{
  val arr = val::array();
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (arr.set((int)I, to_js(std::get<I>(v))), ...);
  }(std::make_index_sequence<std::tuple_size_v<T>>{});
  return arr;
}

template <avnd::variant_ish T>
inline val to_js(const T& v)
{
  return std::visit([](const auto& alt) { return to_js(alt); }, v);
}

template <avnd::bitset_ish T>
inline val to_js(const T& v)
{
  val arr = val::array();
  for(int i = 0, N = (int)v.size(); i < N; i++)
    arr.set(i, val(v.test(i)));
  return arr;
}

template <avnd::map_ish T>
inline val to_js(const T& v)
{
  // Maps with string keys -> JS object; otherwise -> array of [key,value].
  if constexpr(avnd::string_ish<typename T::key_type>)
  {
    val o = val::object();
    for(const auto& [k, val_] : v)
      o.set(std::string{k}, to_js(val_));
    return o;
  }
  else
  {
    val arr = val::array();
    int i = 0;
    for(const auto& [k, val_] : v)
    {
      val pair = val::array();
      pair.set(0, to_js(k));
      pair.set(1, to_js(val_));
      arr.set(i++, std::move(pair));
    }
    return arr;
  }
}

template <typename T>
  requires((avnd::set_ish<T> || avnd::list_ish<T> || avnd::span_ish<T>) && !avnd::string_ish<T> && !avnd::map_ish<T> && !avnd::bitset_ish<T>)
inline val to_js(const T& v)
{
  val arr = val::array();
  int i = 0;
  for(const auto& e : v)
    arr.set(i++, to_js(e));
  return arr;
}

// ---------------------------------------------------------------------------
// emscripten::val -> native value
// ---------------------------------------------------------------------------

inline bool js_is_array(const val& v)
{
  return v.isArray() || (!v.isNull() && !v.isUndefined() && v.instanceof(val::global("Array")));
}

// Scalars / strings / enums.
template <typename T>
inline void from_js(T& out, const val& v);

template <typename T>
  requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
inline void from_js_scalar(T& out, const val& v)
{
  if(v.isString())
  {
    // Accept numeric strings.
    const std::string s = v.as<std::string>();
    if constexpr(std::is_floating_point_v<T>)
      out = static_cast<T>(std::strtod(s.c_str(), nullptr));
    else
      out = static_cast<T>(std::strtoll(s.c_str(), nullptr, 10));
  }
  else if(v.isNull() || v.isUndefined())
  {
    out = T{};
  }
  else
  {
    out = static_cast<T>(v.as<double>());
  }
}

template <typename T>
inline void from_js(T& out, const val& v)
{
  using D = std::decay_t<T>;
  if constexpr(std::is_same_v<D, bool>)
  {
    if(v.isNumber())
      out = v.as<double>() != 0.0;
    else if(v.isString())
    {
      const std::string s = v.as<std::string>();
      out = (s == "true" || s == "1");
    }
    else if(v.isNull() || v.isUndefined())
      out = false;
    else
      out = v.as<bool>();
  }
  else if constexpr(std::is_enum_v<D>)
  {
    if(v.isString())
    {
      if(auto e = magic_enum::enum_cast<D>(v.as<std::string>()))
        out = *e;
    }
    else if(v.isNumber())
    {
      out = static_cast<D>(static_cast<int>(v.as<double>()));
    }
  }
  else if constexpr(std::is_same_v<D, std::string>)
  {
    if(v.isString())
      out = v.as<std::string>();
    else if(v.isNumber())
      out = std::to_string(v.as<double>());
    else if(v.isNull() || v.isUndefined())
      out.clear();
  }
  else if constexpr(std::is_arithmetic_v<D>)
  {
    from_js_scalar(out, v);
  }
  else if constexpr(avnd::optional_ish<D>)
  {
    if(v.isNull() || v.isUndefined())
    {
      out.reset();
    }
    else
    {
      typename D::value_type tmp{};
      from_js(tmp, v);
      out = std::move(tmp);
    }
  }
  else if constexpr(avnd::bitset_ish<D>)
  {
    out.reset();
    if(js_is_array(v))
    {
      const int n = std::min<int>(v["length"].as<int>(), (int)out.size());
      for(int i = 0; i < n; i++)
      {
        const val e = v[i];
        bool b = false;
        from_js(b, e);
        if(b)
          out.set(i);
      }
    }
  }
  else if constexpr(avnd::pair_ish<D>)
  {
    if(js_is_array(v))
    {
      val a0 = v[0], a1 = v[1];
      from_js(out.first, a0);
      from_js(out.second, a1);
    }
  }
  else if constexpr(avnd::tuple_ish<D> && !avnd::iterable_ish<D>)
  {
    if(js_is_array(v))
    {
      [&]<std::size_t... I>(std::index_sequence<I...>) {
        ([&] {
          val e = v[(int)I];
          from_js(std::get<I>(out), e);
        }(),
         ...);
      }(std::make_index_sequence<std::tuple_size_v<D>>{});
    }
  }
  else if constexpr(avnd::variant_ish<D>)
  {
    // Pick the alternative matching the JS runtime type.
    if(v.isString())
    {
      if constexpr(requires { out = std::string{}; })
        out = v.as<std::string>();
    }
    else if(v.isNumber())
    {
      // Prefer the first arithmetic alternative.
      bool done = false;
      [&]<std::size_t... I>(std::index_sequence<I...>) {
        (([&] {
           using Alt = std::variant_alternative_t<I, D>;
           if(!done && std::is_arithmetic_v<Alt>)
           {
             Alt tmp{};
             if constexpr(std::is_arithmetic_v<Alt>)
               tmp = static_cast<Alt>(v.as<double>());
             out = tmp;
             done = true;
           }
         }()),
         ...);
      }(std::make_index_sequence<std::variant_size_v<D>>{});
    }
  }
  else if constexpr(avnd::map_ish<D>)
  {
    out.clear();
    using key_t = std::remove_cvref_t<typename D::key_type>;
    using mapped_t = std::remove_cvref_t<typename D::mapped_type>;
    if constexpr(avnd::string_ish<key_t>)
    {
      // JS object { key: value }
      const val keys = val::global("Object").call<val>("keys", v);
      const int n = keys["length"].as<int>();
      for(int i = 0; i < n; i++)
      {
        const std::string k = keys[i].as<std::string>();
        const val ev = v[k];
        mapped_t mv{};
        from_js(mv, ev);
        out.emplace(key_t{k}, std::move(mv));
      }
    }
    else if(js_is_array(v))
    {
      // Array of [key, value] pairs.
      const int n = v["length"].as<int>();
      for(int i = 0; i < n; i++)
      {
        const val pair = v[i];
        key_t k{};
        mapped_t mv{};
        val pk = pair[0], pv = pair[1];
        from_js(k, pk);
        from_js(mv, pv);
        out.emplace(std::move(k), std::move(mv));
      }
    }
  }
  else if constexpr(avnd::vector_ish<D> || avnd::list_ish<D>)
  {
    out.clear();
    if(js_is_array(v))
    {
      const int n = v["length"].as<int>();
      if constexpr(requires { out.reserve(n); })
        out.reserve(n);
      for(int i = 0; i < n; i++)
      {
        typename D::value_type e{};
        val ev = v[i];
        from_js(e, ev);
        out.push_back(std::move(e));
      }
    }
  }
  else if constexpr(avnd::set_ish<D>)
  {
    out.clear();
    if(js_is_array(v))
    {
      const int n = v["length"].as<int>();
      for(int i = 0; i < n; i++)
      {
        typename D::value_type e{};
        val ev = v[i];
        from_js(e, ev);
        out.insert(std::move(e));
      }
    }
  }
  else if constexpr(avnd::array_ish<D, 1> || (avnd::span_ish<D> && avnd::static_array_ish<D>))
  {
    // Fixed-size array: assign element-by-element up to its size.
    if(js_is_array(v))
    {
      const int n = std::min<int>(v["length"].as<int>(), (int)std::size(out));
      for(int i = 0; i < n; i++)
      {
        val ev = v[i];
        from_js(out[i], ev);
      }
    }
  }
  else if constexpr(avnd::xyzw_value<D> || avnd::xyz_value<D> || avnd::xy_value<D> || avnd::rgba_value<D> || avnd::rgb_value<D>)
  {
    // Accept either {x,y,..}/{r,g,b,a} object or [..] array.
    const bool isArr = js_is_array(v);
    auto get = [&](int idx, const char* key) -> val {
      return isArr ? v[idx] : v[key];
    };
    if constexpr(avnd::rgba_value<D>)
    {
      val r = get(0, "r"), g = get(1, "g"), b = get(2, "b"), a = get(3, "a");
      from_js(out.r, r);
      from_js(out.g, g);
      from_js(out.b, b);
      from_js(out.a, a);
    }
    else if constexpr(avnd::rgb_value<D>)
    {
      val r = get(0, "r"), g = get(1, "g"), b = get(2, "b");
      from_js(out.r, r);
      from_js(out.g, g);
      from_js(out.b, b);
    }
    else if constexpr(avnd::xyzw_value<D>)
    {
      val x = get(0, "x"), y = get(1, "y"), z = get(2, "z"), w = get(3, "w");
      from_js(out.x, x);
      from_js(out.y, y);
      from_js(out.z, z);
      from_js(out.w, w);
    }
    else if constexpr(avnd::xyz_value<D>)
    {
      val x = get(0, "x"), y = get(1, "y"), z = get(2, "z");
      from_js(out.x, x);
      from_js(out.y, y);
      from_js(out.z, z);
    }
    else if constexpr(avnd::xy_value<D>)
    {
      val x = get(0, "x"), y = get(1, "y");
      from_js(out.x, x);
      from_js(out.y, y);
    }
  }
  else if constexpr(std::is_aggregate_v<D> && avnd::pfr::tuple_size_v<D> > 0)
  {
    // Generic struct: array of fields, or object via field index.
    if(js_is_array(v))
    {
      int i = 0;
      avnd::for_each_field_ref(out, [&]<typename F>(F& field) {
        val ev = v[i++];
        from_js(field, ev);
      });
    }
  }
  // else: empty aggregate (bang) / unsupported -> leave default.
}

#endif // __EMSCRIPTEN__

// ---------------------------------------------------------------------------
// Parameter / output descriptor + dispatch helpers (compile under both
// emscripten and native; the val-returning ones are guarded individually).
// ---------------------------------------------------------------------------

// Logical "type" string a JS UI can switch on.
template <typename Field>
constexpr std::string_view js_value_type()
{
  using V = std::decay_t<decltype(std::declval<Field>().value)>;
  if constexpr(avnd::enum_parameter<Field> || avnd::enum_ish_parameter<Field>)
    return "enum";
  else if constexpr(avnd::bool_parameter<Field>)
    return "bool";
  else if constexpr(avnd::int_parameter<Field>)
    return "int";
  else if constexpr(avnd::float_parameter<Field>)
    return "float";
  else if constexpr(avnd::string_parameter<Field>)
    return "string";
  else if constexpr(avnd::rgba_parameter<Field>)
    return "rgba";
  else if constexpr(avnd::rgb_parameter<Field>)
    return "rgb";
  else if constexpr(avnd::xyzw_parameter<Field>)
    return "xyzw";
  else if constexpr(avnd::xyz_parameter<Field>)
    return "xyz";
  else if constexpr(avnd::xy_parameter<Field>)
    return "xy";
  else if constexpr(avnd::optional_ish<V>)
    return "optional";
  else if constexpr(avnd::variant_ish<V>)
    return "variant";
  else if constexpr(avnd::map_ish<V>)
    return "map";
  else if constexpr(avnd::bitset_ish<V>)
    return "bitset";
  else if constexpr(avnd::pair_ish<V>)
    return "pair";
  else if constexpr(avnd::tuple_ish<V> && !avnd::iterable_ish<V>)
    return "tuple";
  else if constexpr((avnd::vector_ish<V> || avnd::list_ish<V> || avnd::set_ish<V> || avnd::span_ish<V>) && !avnd::string_ish<V>)
    return "list";
  else if constexpr(std::is_aggregate_v<V> && avnd::pfr::tuple_size_v<V> == 0)
    return "bang";
  else if constexpr(std::is_aggregate_v<V>)
    return "struct";
  else
    return "unknown";
}

#if defined(__EMSCRIPTEN__)

// Number of "components" for compound vec/color controls (for the JS UI).
template <typename Field>
constexpr int js_component_count()
{
  if constexpr(avnd::rgba_parameter<Field> || avnd::xyzw_parameter<Field>)
    return 4;
  else if constexpr(avnd::rgb_parameter<Field> || avnd::xyz_parameter<Field>)
    return 3;
  else if constexpr(avnd::xy_parameter<Field>)
    return 2;
  else
    return 1;
}

// Fill min/max/init/step + values[]/components on the descriptor for a Field.
template <typename Field>
inline void fill_range_info(val& o)
{
  using V = std::decay_t<decltype(std::declval<Field>().value)>;

  // Enum / values-range -> choices[]
  if constexpr(avnd::enum_ish_parameter<Field>)
  {
    val choices = val::array();
    if constexpr(avnd::enum_parameter<Field> && avnd::get_enum_choices_count<Field>() > 0 && std::tuple_size_v<std::decay_t<decltype(avnd::get_enum_choices<Field>())>> == 0)
    {
      // Plain enum without a values[] range: use magic_enum.
      using E = std::decay_t<decltype(std::declval<Field>().value)>;
      int i = 0;
      for(auto&& name : magic_enum::enum_names<E>())
        choices.set(i++, std::string{name});
    }
    else
    {
      constexpr auto names = avnd::get_enum_choices<Field>();
      int i = 0;
      for(auto& n : names)
        choices.set(i++, std::string{n});
    }
    o.set("values", choices);
    o.set("min", val(0));
    o.set("max", val((int)avnd::get_enum_choices_count<Field>() - 1));
  }
  else if constexpr(avnd::parameter_with_minmax_range<Field>)
  {
    constexpr auto r = avnd::get_range<Field>();
    o.set("min", to_js(r.min));
    o.set("max", to_js(r.max));
    o.set("init", to_js(r.init));
    if constexpr(requires { r.step; })
      o.set("step", to_js(r.step));
  }
  else if constexpr(avnd::parameter_with_minmax_range_ignore_init<Field>)
  {
    constexpr auto r = avnd::get_range<Field>();
    o.set("min", to_js(r.min));
    o.set("max", to_js(r.max));
  }
}

// Build the descriptor object for parameter at predicate-index "idx".
template <typename Field>
inline val make_param_info(int idx)
{
  using F = std::decay_t<Field>;
  val o = val::object();
  o.set("index", val(idx));

  // identifier (C identifier) + display name
  static constexpr auto ident = avnd::get_c_identifier<F>();
  o.set("identifier", val(std::string{std::string_view{ident}}));
  if constexpr(avnd::has_name<F>)
    o.set("name", val(std::string{avnd::get_name<F>()}));
  else
    o.set("name", val(std::string{std::string_view{ident}}));

  // widget
  static constexpr auto w = avnd::get_widget<F>();
  o.set("widget", val(std::string{w.name()}));

  // "control" = parameter with a widget (knob/slider); "value" = parameter port
  // with a .value but no widget. The JS UI groups them differently.
  if constexpr(avnd::control_port<F>)
    o.set("kind", val(std::string{"control"}));
  else
    o.set("kind", val(std::string{"value"}));

  // logical type
  o.set("type", val(std::string{js_value_type<F>()}));

  // unit
  if constexpr(avnd::has_unit<F>)
    o.set("unit", val(std::string{avnd::get_unit<F>()}));

  // components for compound controls
  if constexpr(js_component_count<F>() > 1)
    o.set("components", val(js_component_count<F>()));

  fill_range_info<F>(o);
  return o;
}

// ---------------------------------------------------------------------------
// Per-field set / get (real, un-normalized values)
// ---------------------------------------------------------------------------

// Set a field from a JS value. Scalars/enums/strings go through avnd::apply_control
// (range clamping/snapping); structured types go through from_js().
template <typename Field, typename Effect>
inline void set_field(Field& ctl, Effect& eff, const val& v)
{
  if constexpr(avnd::bool_parameter<Field>)
  {
    bool b{};
    from_js(b, v);
    avnd::apply_control(ctl, b ? 1 : 0);
  }
  else if constexpr(avnd::enum_parameter<Field>)
  {
    using E = std::decay_t<decltype(Field::value)>;
    if(v.isString())
    {
      const std::string s = v.as<std::string>();
      // By enum name; for values[]-range enums fall back to the range-aware setter.
      if(auto e = magic_enum::enum_cast<E>(s))
        ctl.value = *e;
      else if constexpr(avnd::parameter_with_values_range<Field>)
        avnd::apply_control(ctl, s);
    }
    else
    {
      ctl.value = static_cast<E>(static_cast<int>(v.as<double>()));
    }
  }
  else if constexpr(avnd::int_parameter<Field>)
  {
    long long iv{};
    from_js(iv, v);
    avnd::apply_control(ctl, iv);
  }
  else if constexpr(avnd::float_parameter<Field>)
  {
    double fv{};
    from_js(fv, v);
    avnd::apply_control(ctl, fv);
  }
  else if constexpr(avnd::string_parameter<Field>)
  {
    if(v.isString())
      avnd::apply_control(ctl, v.as<std::string>());
  }
  else
  {
    // Compound / container / structured value.
    from_js(ctl.value, v);
  }

  if_possible(ctl.update(eff));
}

// Read a field back out as a JS value.
template <typename Field>
inline val get_field(Field& ctl)
{
  return to_js(ctl.value);
}

// Set a field from a normalized [0;1] value (scalars/enums only; structured
// types fall back to from_js of the raw number where meaningful).
template <typename Field, typename Effect>
inline void set_field_normalized(Field& ctl, Effect& eff, double v01)
{
  if constexpr(requires { avnd::map_control_from_01<Field>(v01); })
  {
    ctl.value = static_cast<std::decay_t<decltype(Field::value)>>(
        avnd::map_control_from_01<Field>(v01));
    if_possible(ctl.update(eff));
  }
}

// Read a field back out as a normalized [0;1] value.
template <typename Field>
inline double get_field_normalized(Field& ctl)
{
  if constexpr(requires { avnd::map_control_to_01<Field>(ctl.value); })
    return (double)avnd::map_control_to_01<Field>(ctl.value);
  else
    return 0.0;
}

#endif // __EMSCRIPTEN__

} // namespace wasm
