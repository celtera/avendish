#pragma once
#include <avnd/binding/max/atom_helpers.hpp>
#include <avnd/binding/max/from_atoms.hpp>
#include <avnd/binding/max/to_atoms.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/concepts/field_names.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <ext.h>

namespace max
{

template <typename Processor, typename T>
struct attribute_register;

template <typename Processor, typename T>
  requires(avnd::attribute_input_introspection<T>::size == 0)
struct attribute_register<Processor, T>
{
  void operator()(auto&&) const noexcept { }
};

template <typename Processor, typename T>
  requires (avnd::attribute_input_introspection<T>::size > 0)
struct attribute_register<Processor, T>
{
  using attrs = avnd::attribute_input_introspection<T>;
  static constexpr int num_attributes = attrs::size;

  t_class* c{};
  std::array<t_object*, num_attributes>& attributes;

  template<std::size_t I>
  static t_max_err getter(Processor* x, void*, long* ac, t_atom** av)
  {
    auto& ins = avnd::get_inputs(x->implementation.effect);
    auto& field = avnd::input_introspection<T>::template field<I>(ins);

    return to_atoms{ac, av}(field.value);
  }

  template<std::size_t I>
  static t_max_err setter(Processor *x, void*, long ac, t_atom *av)
  {
    auto& obj = x->implementation.effect;
    auto& ins = avnd::get_inputs(obj);
    auto& field = avnd::input_introspection<T>::template field<I>(ins);
    if (ac && av) {
      if constexpr(convertible_to_atom_list_statically<decltype(field.value)>)
      {
        if(from_atoms{ac, av}(field.value))
        {
          if_possible(field.update(obj));
        }
      }
      else
      {
        // FIXME TODO
      }
    }
    else
    {
      // attribute name without argument: unset it
      field.value = {};
      if constexpr(requires { field.update(obj); })
      {
        field.update(obj);
      }
    }
    return MAX_ERR_NONE;
  }

  template<std::size_t I, typename F>
  void operator()(avnd::field_reflection<I, F>) {
    using V = decltype(F::value);

    if(t_symbol* sym = get_atoms_sym<V>())
    {
      static constexpr auto attr_idx = attrs::template unmap<I>();
      static constexpr auto attr_name = max::get_name_symbol<F>();
      static constexpr auto label = avnd::get_name<F>();
      static constexpr auto get = &attribute_register::getter<I>;
      static constexpr auto set = &attribute_register::setter<I>;
      auto attr = attribute_new(attr_name.data(), sym, 0, (method)get, (method)set);
      attributes[attr_idx] = attr;
      class_addattr(c, attr);
      CLASS_ATTR_LABEL(c, attr_name.data(), 0, label.data());

      if(static constexpr auto style = get_atoms_style<F>(); strlen(style) > 0)
        CLASS_ATTR_STYLE(c, attr_name.data(), 0, style);

      if constexpr(avnd::parameter_with_minmax_range<V>)
      {
        static constexpr auto range = avnd::get_range<V>();
        CLASS_ATTR_FILTER_CLIP(c, attr_name.data(), range.min, range.max);
        const auto init = std::to_string(range.init);
        CLASS_ATTR_DEFAULT(c, attr_name.data(), 0, init.c_str());
      }

      // TODO:
      // CLASS_ATTR_SAVE(c, attrname, flags) < saved by the patcher
      // CLASS_ATTR_SELFSAVE(c, attrname, flags) < saved in its own way
      // CLASS_ATTR_ENUM(c, attrname, flags, parsestr) < parsestr == "v1 v2 v3"
      // CLASS_ATTR_ENUMINDEX(c, attrname, flags, parsestr)
      // CLASS_ATTR_CATEGORY(c, attrname, flags, parsestr)
      // CLASS_ATTR_BASIC(c, attrname, flags)
      // CLASS_ATTR_OBSOLETE(c, attrname, flags)
      // CLASS_ATTR_INTRODUCED(c, attrname, flags, versionstr)
      // CLASS_ATTR_INVISIBLE(c,attrname,flags)
    }
  }
};

template <typename Processor, typename T>
struct attribute_object_register;

template <typename Processor, typename T>
  requires(avnd::attribute_input_introspection<T>::size == 0)
struct attribute_object_register<Processor, T>
{
  void operator()(auto&&) const noexcept { }
};

template <typename Processor, typename T>
  requires (avnd::attribute_input_introspection<T>::size > 0)
struct attribute_object_register<Processor, T>
{
  using attrs = avnd::attribute_input_introspection<T>;
  static constexpr int num_attributes = attrs::size;

  t_object* o{};

  template<typename F, std::size_t I>
  void operator()(F& field, avnd::predicate_index<I>)
  {
    static const auto attr_name = max::symbol_from_name<F>();

    if constexpr(std::is_integral_v<decltype(F::value)>)
    {
      object_attr_setlong(o, attr_name, field.value);
    }
    else if constexpr(std::is_floating_point_v<decltype(F::value)>)
    {
      object_attr_setfloat(o, attr_name, field.value);
    }
    else if constexpr(avnd::string_ish<decltype(F::value)>)
    {
      object_attr_setsym(o, attr_name, gensym(field.value.data()));
    }
    else
    {
      static_assert(F::error_in_attribute, "Unhandled attribute type");
    }
  }
};
}
