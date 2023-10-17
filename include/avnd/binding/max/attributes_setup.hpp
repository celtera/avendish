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
struct attribute_register
{
  t_class* c{};

  template<std::size_t I>
  static t_max_err getter(Processor* x, void*, long* ac, t_atom** av)
  {
    auto& ins = avnd::get_inputs(x->implementation.effect);
    auto& field = avnd::input_introspection<T>::template field<I>(ins);

    return to_atoms{ac, av}(field.value);
  }

  template<std::size_t I>
  static t_max_err setter(Processor *x, void *attr, long ac, t_atom *av)
  {
    if (ac && av) {
      auto& obj = x->implementation.effect;
      auto& ins = avnd::get_inputs(obj);
      auto& field = avnd::input_introspection<T>::template field<I>(ins);

      if(from_atoms{ac, av}(field.value))
      {
        if constexpr(requires { field.update(obj); })
        {
          field.update(obj);
        }
      }
    }
    return MAX_ERR_NONE;
  };

  template<std::size_t I, typename F>
  void operator()(avnd::field_reflection<I, F>) {
    using V = decltype(F::value);
    static constexpr auto name = avnd::get_name<F>();

    t_symbol* sym = get_atoms_sym<V>();
    if(sym)
    {
      static constexpr auto get = &attribute_register::getter<I>;
      static constexpr auto set = &attribute_register::setter<I>;
      std::string_view attr_name;
      if constexpr(avnd::has_c_name<F>)
        attr_name = avnd::get_c_name<F>();
      else
        attr_name = name;

      auto attr = attribute_new(attr_name.data(), sym, 0, (method)get, (method)set);
      class_addattr(c, attr);
      CLASS_ATTR_LABEL(c, attr_name.data(), 0, name.data());

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

}
