#pragma once
#include <avnd/binding/max/from_atoms.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/field_names.hpp>
namespace max
{
// FIXME recursive dicts not handled right now
struct from_dict
{
  template <typename V>
    requires avnd::has_field_names<std::remove_cvref_t<V>>
  void operator()(t_dictionary* d, V&& v) noexcept
  {
    v.clear();

    avnd::for_each_field_ref_n(
        v, [&]<std::size_t N>(const auto& f, avnd::field_index<N>) {
      static constexpr auto name = std::remove_cvref_t<V>::field_names()[N];
      static const auto key = gensym(name.data());

      long argc{};
      t_atom* argv{};
      if(dictionary_getatoms(d, key, &argc, &argv) == MAX_ERR_NONE)
      {
        from_atoms{argc, argv}(v[name]);
      }
      else if(t_atom value; dictionary_getatom(d, key, &value) == MAX_ERR_NONE)
      {
        from_atom{value}(v[name]);
      }
    });
  }

  template <avnd::dict_ish V>
  void operator()(t_dictionary* d, V& v)
  {
    v.clear();

    ::dictionary_funall(d, [](t_dictionary_entry* entry, void* my_arg) {
      auto& v = *static_cast<V*>(my_arg);

      // Get the data
      t_symbol* key = dictionary_entry_getkey(entry);

      long argc{};
      t_atom* argv{};
      dictionary_entry_getvalues(entry, &argc, &argv);
      if(argc > 0)
      {
        from_atoms{argc, argv}(v[key->s_name]);
      }
      else
      {
        t_atom value;
        dictionary_entry_getvalue(entry, &value);
        if(value.a_type != A_NOTHING)
          from_atom{value}(v[key->s_name]);
      }
    }, &v);
  }
};
}
