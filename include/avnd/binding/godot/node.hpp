#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/generic.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/introspection/widgets.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <magic_enum/magic_enum.hpp>

#include <string>
#include <type_traits>

namespace godot_binding
{

/// Get the property name for a field.
/// Uses the explicit name() if the field type declares one,
/// otherwise falls back to the PFR struct member name (e.g. "a", "b").
/// This is critical for anonymous types: two fields like
///   struct { float value; } a;
///   struct { float value; } b;
/// have the same type C, so avnd::get_name<C>() returns the same string.
/// PFR field names are index-based and always unique.
template <auto Idx, typename C, typename Parent>
constexpr std::string_view field_name()
{
  if constexpr(avnd::has_name<C>)
    return avnd::get_name<C>();
  else
    return boost::pfr::get_name<Idx, Parent>();
}

/// Map an Avendish parameter field to a Godot Variant::Type
template <typename C>
constexpr godot::Variant::Type variant_type()
{
  // FIXME handle other more complicated types if any?
  if constexpr(avnd::rgba_parameter<C>)
    return godot::Variant::COLOR;
  else if constexpr(avnd::rgb_parameter<C>)
    return godot::Variant::COLOR;
  else if constexpr(avnd::xyz_parameter<C>)
    return godot::Variant::VECTOR3;
  else if constexpr(avnd::xy_parameter<C>)
    return godot::Variant::VECTOR2;
  else if constexpr(avnd::enum_parameter<C>)
    return godot::Variant::INT;
  else if constexpr(avnd::float_parameter<C>)
    return godot::Variant::FLOAT;
  else if constexpr(avnd::int_parameter<C>)
    return godot::Variant::INT;
  else if constexpr(avnd::bool_parameter<C>)
    return godot::Variant::BOOL;
  else if constexpr(avnd::string_parameter<C>)
    return godot::Variant::STRING;
  else
    return godot::Variant::NIL;
}

/// Build a Godot range hint string ("min,max,step") for numeric parameters
template <typename C>
godot::String range_hint_string()
{
  if constexpr(avnd::parameter_with_minmax_range<C>)
  {
    static constexpr auto range = avnd::get_range<C>();
    if constexpr(avnd::float_parameter<C>)
    {
      return godot::String::num(range.min) + "," + godot::String::num(range.max)
             + ",0.01";
    }
    else if constexpr(avnd::int_parameter<C>)
    {
      return godot::String::num(range.min) + "," + godot::String::num(range.max) + ",1";
    }
  }
  return {};
}

/// Build a Godot enum hint string ("Val1,Val2,Val3") for enum parameters.
/// Uses avnd::get_enum_choices<C>() which handles all enum-ish types uniformly
/// (true enums, string arrays, combo_pair arrays, etc.) via to_string_view_array.
template <typename C>
godot::String enum_hint_string()
{
  static constexpr auto choices = avnd::get_enum_choices<C>();

  if constexpr(choices.size() > 0)
  {
    godot::String hint;
    for(int i = 0; i < std::ssize(choices); ++i)
    {
      if(i > 0)
        hint += ",";
      hint += choices[i].data();
    }
    return hint;
  }
  else if constexpr(avnd::enum_parameter<C>)
  {
    // Fallback: true C++ enum without explicit range — use magic_enum
    using enum_type = std::decay_t<decltype(C::value)>;
    static constexpr auto entries = magic_enum::enum_entries<enum_type>();
    godot::String hint;
    for(int i = 0; i < std::ssize(entries); ++i)
    {
      if(i > 0)
        hint += ",";
      hint += entries[i].second.data();
    }
    return hint;
  }

  return {};
}

/// Build a PropertyInfo for a given Avendish parameter field
template <auto Idx, typename C, typename Parent>
godot::PropertyInfo make_property_info()
{
  auto name = godot::StringName(field_name<Idx, C, Parent>().data());
  auto type = variant_type<C>();

  if constexpr(avnd::enum_parameter<C>)
  {
    return godot::PropertyInfo(
        godot::Variant::INT, name, godot::PROPERTY_HINT_ENUM, enum_hint_string<C>());
  }
  else if constexpr(avnd::enum_ish_parameter<C>)
  {
    return godot::PropertyInfo(
        godot::Variant::INT, name, godot::PROPERTY_HINT_ENUM, enum_hint_string<C>());
  }
  else if constexpr(avnd::parameter_with_minmax_range<C>)
  {
    return godot::PropertyInfo(
        type, name, godot::PROPERTY_HINT_RANGE, range_hint_string<C>());
  }
  else
  {
    return godot::PropertyInfo(type, name);
  }
}

/// Set an Avendish field's value from a Godot Variant
template <typename C>
bool set_from_variant(C& field, const godot::Variant& v)
{
  using value_type = std::decay_t<decltype(C::value)>;

  if constexpr(avnd::rgba_parameter<C>)
  {
    godot::Color col = v;
    using comp_t = std::decay_t<decltype(field.value.r)>;
    field.value.r = static_cast<comp_t>(col.r);
    field.value.g = static_cast<comp_t>(col.g);
    field.value.b = static_cast<comp_t>(col.b);
    field.value.a = static_cast<comp_t>(col.a);
  }
  else if constexpr(avnd::rgb_parameter<C>)
  {
    godot::Color col = v;
    using comp_t = std::decay_t<decltype(field.value.r)>;
    field.value.r = static_cast<comp_t>(col.r);
    field.value.g = static_cast<comp_t>(col.g);
    field.value.b = static_cast<comp_t>(col.b);
  }
  else if constexpr(avnd::xyz_parameter<C>)
  {
    godot::Vector3 vec = v;
    using comp_t = std::decay_t<decltype(field.value.x)>;
    field.value.x = static_cast<comp_t>(vec.x);
    field.value.y = static_cast<comp_t>(vec.y);
    field.value.z = static_cast<comp_t>(vec.z);
  }
  else if constexpr(avnd::xy_parameter<C>)
  {
    godot::Vector2 vec = v;
    using comp_t = std::decay_t<decltype(field.value.x)>;
    field.value.x = static_cast<comp_t>(vec.x);
    field.value.y = static_cast<comp_t>(vec.y);
  }
  else if constexpr(avnd::enum_parameter<C>)
  {
    field.value = static_cast<value_type>(int64_t(v));
  }
  else if constexpr(avnd::float_parameter<C>)
  {
    field.value = static_cast<value_type>(double(v));
  }
  else if constexpr(avnd::int_parameter<C>)
  {
    field.value = static_cast<value_type>(int64_t(v));
  }
  else if constexpr(avnd::bool_parameter<C>)
  {
    field.value = bool(v);
  }
  else if constexpr(avnd::string_parameter<C>)
  {
    godot::String gstr = v;
    godot::CharString cs = gstr.utf8();
    field.value = cs.get_data();
  }
  else
  {
    return false;
  }

  return true;
}

/// Convert an Avendish field's value to a Godot Variant
template <typename C>
godot::Variant to_variant(const C& field)
{
  if constexpr(avnd::rgba_parameter<C>)
  {
    return godot::Color(field.value.r, field.value.g, field.value.b, field.value.a);
  }
  else if constexpr(avnd::rgb_parameter<C>)
  {
    return godot::Color(field.value.r, field.value.g, field.value.b, 1.0f);
  }
  else if constexpr(avnd::xyz_parameter<C>)
  {
    return godot::Vector3(field.value.x, field.value.y, field.value.z);
  }
  else if constexpr(avnd::xy_parameter<C>)
  {
    return godot::Vector2(field.value.x, field.value.y);
  }
  else if constexpr(avnd::enum_parameter<C>)
  {
    return int64_t(field.value);
  }
  else if constexpr(avnd::float_parameter<C>)
  {
    return double(field.value);
  }
  else if constexpr(avnd::int_parameter<C>)
  {
    return int64_t(field.value);
  }
  else if constexpr(avnd::bool_parameter<C>)
  {
    return bool(field.value);
  }
  else if constexpr(avnd::string_parameter<C>)
  {
    return godot::String(std::string(field.value).c_str());
  }
  else
  {
    return {};
  }
}

/// Populate the Godot property list from Avendish introspection
template <typename T>
void get_property_list(godot::List<godot::PropertyInfo>* p_list)
{
  if constexpr(avnd::has_inputs<T>)
  {
    using inputs_t = typename avnd::inputs_type<T>::type;
    avnd::parameter_input_introspection<T>::for_all(
        [p_list]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      p_list->push_back(make_property_info<Idx, C, inputs_t>());
    });
  }

  if constexpr(avnd::has_outputs<T>)
  {
    using outputs_t = typename avnd::outputs_type<T>::type;
    avnd::parameter_output_introspection<T>::for_all(
        [p_list]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      auto info = make_property_info<Idx, C, outputs_t>();
      info.usage = godot::PROPERTY_USAGE_EDITOR | godot::PROPERTY_USAGE_READ_ONLY;
      p_list->push_back(info);
    });
  }
}

/// Set a property on the effect by name
template <typename T>
bool set_property(
    avnd::effect_container<T>& effect, const godot::StringName& p_name,
    const godot::Variant& p_value)
{
  if constexpr(!avnd::has_inputs<T>)
    return false;

  using inputs_t = typename avnd::inputs_type<T>::type;
  bool found = false;
  avnd::parameter_input_introspection<T>::for_all(
      [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
    if(!found)
    {
      static const godot::StringName fname{field_name<Idx, C, inputs_t>().data()};
      if(fname == p_name)
      {
        auto& fld = avnd::pfr::get<Idx>(effect.inputs());
        if(set_from_variant<C>(fld, p_value))
        {
          if_possible(fld.update(effect.effect));
          found = true;
        }
      }
    }
  });
  return found;
}

/// Get a property from the effect by name
template <typename T>
bool get_property(
    avnd::effect_container<T>& effect, const godot::StringName& p_name,
    godot::Variant& r_ret)
{
  bool found = false;

  if constexpr(avnd::has_inputs<T>)
  {
    using inputs_t = typename avnd::inputs_type<T>::type;
    avnd::parameter_input_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      if(!found)
      {
        static const godot::StringName fname{field_name<Idx, C, inputs_t>().data()};
        if(fname == p_name)
        {
          r_ret = to_variant<C>(avnd::pfr::get<Idx>(effect.inputs()));
          found = true;
        }
      }
    });
  }

  if(!found)
  {
    if constexpr(avnd::has_outputs<T>)
    {
      using outputs_t = typename avnd::outputs_type<T>::type;
      avnd::parameter_output_introspection<T>::for_all(
          [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
        if(!found)
        {
          static const godot::StringName fname{field_name<Idx, C, outputs_t>().data()};
          if(fname == p_name)
          {
            r_ret = to_variant<C>(avnd::pfr::get<Idx>(effect.outputs()));
            found = true;
          }
        }
      });
    }
  }

  return found;
}

/// Check if a property can revert to default
template <typename T>
bool property_can_revert(const godot::StringName& p_name)
{
  bool found = false;

  if constexpr(avnd::has_inputs<T>)
  {
    using inputs_t = typename avnd::inputs_type<T>::type;
    avnd::parameter_input_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      if constexpr(avnd::has_range<C>)
      {
        if(!found)
        {
          static const godot::StringName fname{field_name<Idx, C, inputs_t>().data()};
          if(fname == p_name)
            found = true;
        }
      }
    });
  }

  return found;
}

/// Get the default value for a property
template <typename T>
bool property_get_revert(const godot::StringName& p_name, godot::Variant& r_property)
{
  bool found = false;

  if constexpr(avnd::has_inputs<T>)
  {
    using inputs_t = typename avnd::inputs_type<T>::type;
    avnd::parameter_input_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      if(!found)
      {
        if constexpr(avnd::has_range<C>)
        {
          static const godot::StringName fname{field_name<Idx, C, inputs_t>().data()};
          if(fname == p_name)
          {
            C default_field{};
            static constexpr auto range = avnd::get_range<C>();
            if_possible(default_field.value = range.init);
            r_property = to_variant<C>(default_field);
            found = true;
          }
        }
      }
    });
  }

  return found;
}

/// Invoke the processor's operator()
/// Only call signatures meaningful for a Godot Node (_process with delta).
/// Audio processors with operator()(int N) are handled by godot_audio_effect.
template <typename T>
void invoke_process(avnd::effect_container<T>& effect, double delta)
{
  // Use brace-init to prevent implicit narrowing conversions (e.g. double -> int),
  // so that operator()(int N) audio signatures are not accidentally matched.
  if constexpr(requires { effect.effect({delta}); })
    effect.effect(delta);
  else if constexpr(requires { effect.effect(); })
    effect.effect();
}

/**
 * Godot Node wrapping an Avendish processor.
 *
 * Provides do_* implementation methods. The concrete GDCLASS wrapper
 * must re-declare the Godot virtual overrides (via AVND_GODOT_NODE_OVERRIDES)
 * so that BIND_VIRTUAL_METHOD resolves member pointers to the final class.
 *
 * Usage:
 *   struct my_node final : godot_binding::godot_node<type> {
 *     GDCLASS(my_node, godot::Node);
 *     AVND_GODOT_NODE_OVERRIDES
 *   protected:
 *     static void _bind_methods() {
 *       ClassDB::bind_method(D_METHOD("process"), &my_node::process);
 *     }
 *   };
 */
template <typename T>
struct godot_node : public godot::Node
{
  // mutable: _get is const but effect_container::outputs() may be non-const
  // for monophonic audio processor specializations (coroutine-based iterator)
  mutable avnd::effect_container<T> effect;

  godot_node()
  {
    // Only set default values; do not call update() during construction
    // as the Godot node is not yet fully initialized.
    if constexpr(avnd::has_inputs<T>)
    {
      for(auto state : effect.full_state())
      {
        avnd::for_each_field_ref(state.inputs, [&]<typename Ctl>(Ctl& ctl) {
          if constexpr(avnd::has_range<Ctl>)
          {
            static_constexpr auto c = avnd::get_range<Ctl>();
            if_possible(ctl.value = c.values[c.init].second)
                else if_possible(ctl.value = c.values[c.init])
                else if_possible(ctl.value = c.init);
          }
        });
      }
    }
  }

  ~godot_node() override = default;

  void do_get_property_list(godot::List<godot::PropertyInfo>* p_list) const
  {
    godot_binding::get_property_list<T>(p_list);
  }

  bool do_set(const godot::StringName& p_name, const godot::Variant& p_value)
  {
    return godot_binding::set_property<T>(effect, p_name, p_value);
  }

  bool do_get(const godot::StringName& p_name, godot::Variant& r_ret) const
  {
    return godot_binding::get_property<T>(effect, p_name, r_ret);
  }

  bool do_property_can_revert(const godot::StringName& p_name) const
  {
    return godot_binding::property_can_revert<T>(p_name);
  }

  bool do_property_get_revert(
      const godot::StringName& p_name, godot::Variant& r_property) const
  {
    return godot_binding::property_get_revert<T>(p_name, r_property);
  }

  void do_ready()
  {
    // Now that Godot has fully initialized the node, trigger update() callbacks
    if constexpr(avnd::has_inputs<T>)
    {
      for(auto state : effect.full_state())
      {
        avnd::for_each_field_ref(state.inputs, [&]<typename Ctl>(Ctl& ctl) {
          if_possible(ctl.update(state.effect));
        });
      }
    }
  }

  void do_process(double delta)
  {
    godot_binding::invoke_process<T>(effect, delta);
  }

  void process() { godot_binding::invoke_process<T>(effect, 0.0); }
};

}

/// Virtual override forwarders for godot_node.
/// Must be placed in the concrete GDCLASS body so that BIND_VIRTUAL_METHOD
/// resolves &T::method to a member pointer of the final class type.
// clang-format off
#define AVND_GODOT_NODE_OVERRIDES                                                       \
public:                                                                                 \
  void _ready() override { this->do_ready(); }                                          \
  void _process(double delta) override { this->do_process(delta); }                     \
  void process() { this->do_process(0.0); }                                             \
  void _get_property_list(godot::List<godot::PropertyInfo>* p) const                    \
  { this->do_get_property_list(p); }                                                    \
  bool _set(const godot::StringName& n, const godot::Variant& v)                       \
  { return this->do_set(n, v); }                                                        \
  bool _get(const godot::StringName& n, godot::Variant& r) const                       \
  { return this->do_get(n, r); }                                                        \
  bool _property_can_revert(const godot::StringName& n) const                           \
  { return this->do_property_can_revert(n); }                                           \
  bool _property_get_revert(const godot::StringName& n, godot::Variant& r) const       \
  { return this->do_property_get_revert(n, r); }
// clang-format on
