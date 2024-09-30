#pragma once
#include <avnd/binding/max/helpers.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/field_names.hpp>
#include <boost/container/small_vector.hpp>
#include <ext.h>
#include <magic_enum.hpp>

namespace max
{
struct to_list
{
  boost::container::small_vector<t_atom, 256> atoms;

  to_list()
  {
    atoms.clear();
  }

  void operator()(std::floating_point auto arg)  noexcept
  {
    atom_setfloat(&atoms.emplace_back(), arg);
  }
  void operator()(std::integral auto arg) noexcept
  {
    atom_setlong(&atoms.emplace_back(), arg);
  }

  template <typename T>
    requires std::is_enum_v<T>
  void operator()(T arg) noexcept
  {
    atom_setsym(&atoms.emplace_back(), gensym(magic_enum::enum_name(arg).data()));
  }

  void operator()(std::string_view v) noexcept
  {
    atom_setsym(&atoms.emplace_back(), gensym(v.data()));
  }
  void operator()(const std::string& v)  noexcept
  {
    atom_setsym(&atoms.emplace_back(), gensym(v.data()));
  }
  void operator()(const avnd::variant_ish auto& f)  noexcept
  {
    visit(*this, f);
  }
  void operator()(const avnd::optional_ish auto& f)  noexcept
  {
    if(f)
      (*this)(*f);
    else
      (*this)(0);
  }

  template <typename F>
    requires(std::is_aggregate_v<F> && !avnd::iterable_ish<F> && !avnd::tuple_ish<F>)
  void operator()(const F& f) noexcept
  {
    atoms.reserve(atoms.size() + boost::pfr::tuple_size_v<F>);

    avnd::for_each_field_ref(
        f, [this](const auto& v) mutable{
          (*this)(v);
        });
  }

  void operator()(const avnd::iterable_ish auto& f) noexcept
  {
    atoms.reserve(atoms.size() + f.size());
    for(auto& v : f) {
      (*this)(v);
    }
  }

  void operator()(const avnd::map_ish auto& f)  noexcept
  {
    atoms.reserve(atoms.size() + 2 * f.size());
    {
      for(auto& [key, v] : f) {
        (*this)(key);
        (*this)(v);
      }
    }
  }

  void operator()(const avnd::pair_ish auto& f)  noexcept
  {
    atoms.reserve(atoms.size() + 2);
    (*this)(f.first);
    (*this)(f.second);
  }

  template <avnd::tuple_ish U>
    requires(!avnd::iterable_ish<U>)
  void operator()(const U& f) noexcept
  {
    atoms.reserve(atoms.size() + std::tuple_size_v<U>);
    std::apply(
        [&, k = 0](auto&&... args) mutable { ((*this)(args), ...); },
        f);
  }

  void operator()(const avnd::bitset_ish auto& f)  noexcept
  {
    atoms.reserve(atoms.size() + f.size());
    for(int k = 0; k < f.size(); k++)
      atom_setlong(atoms.data() + k, f.test(k) ? 1 : 0);
  }
};

struct to_dict
{
  t_dictionary* d{};

  void operator()(t_symbol* k, std::floating_point auto&& v) noexcept
  {
    dictionary_appendfloat(d, k, v);
  }

  void operator()(t_symbol* k, std::integral auto&& v) noexcept
  {
    dictionary_appendlong(d, k, v);
  }

  void operator()(t_symbol* k, std::string_view v) noexcept
  {
    dictionary_appendstring(d, k, v.data());
  }

  template <typename T>
    requires std::is_enum_v<T>
  void operator()(t_symbol* k, T&& arg) noexcept
  {
    dictionary_appendstring(d, k, magic_enum::enum_name(arg).data());
  }

  void operator()(t_symbol* k, const avnd::variant_ish auto& f) noexcept
  {
    visit([this, k](auto&& val) { (*this)(k, val); }, f);
  }

  void operator()(t_symbol* k, const avnd::optional_ish auto& f)  noexcept
  {
    if(f)
      (*this)(*f);
    else
      dictionary_deleteentry(d, k);
  }

  template<typename T>
  void operator()(t_symbol* k, T&& v) noexcept
  {
    to_list l;
    l(v);
    dictionary_appendatoms(d, k, l.atoms.size(), l.atoms.data());
  }

  template<typename T>
    requires avnd::has_field_names<std::remove_cvref_t<T>> || avnd::dict_ish<std::remove_cvref_t<T>>
  void operator()(t_symbol* k, T&& v) noexcept
  {
    t_object* cur{};
    bool is_new{};
    if(dictionary_getdictionary(d, k, &cur) != MAX_ERR_NONE || !cur) {
      cur = (t_object*)dictionary_new();
      is_new = true;
    }

    to_dict sub{(t_dictionary*)cur};
    sub(v);

    dictionary_appenddictionary(d, k, cur);
  }

  /// Two functions below are the entrypoints of this mechanism
  // Static case (aggregate with field names)
  template<typename F>
    requires avnd::has_field_names<std::remove_cvref_t<F>>
  void operator()(F&& f)
  {
    avnd::for_each_field_ref_n(
        f, [&]<std::size_t N>(const auto& f, avnd::field_index<N>) {
          static constexpr auto name = std::remove_cvref_t<F>::field_names()[N];
          static const auto sym = gensym(name.data());
          (*this)(sym, f);
        });
  }

  // Dynamic case (std::map<std::string, ...>)
  template<typename F>
    requires avnd::dict_ish<std::remove_cvref_t<F>>
  void operator()(F&& f)
  {
    // 1. Remove entries not in the new map
    {
      t_symbol** keys{};
      long n = 0;
      if(dictionary_getkeys(d, &n, &keys); keys)
      {
        for(long i = 0; i < n; i++)
        {
          if(!f.contains(keys[i]->s_name))
            dictionary_deleteentry(d, keys[i]);
        }
        dictionary_freekeys(d, n, keys);
      }
    }

    // 2. Add all entries in the new map
    for(const auto& [k, v] : f) {
      (*this)(gensym(k.data()), v);
    }
  }
};


struct to_atoms_large
{

};

struct set_atom
{
  t_max_err operator()(t_atom* at, std::integral auto v) const noexcept
  {
    atom_setlong(at, v);
    return MAX_ERR_NONE;
  }

  t_max_err operator()(t_atom* at, std::floating_point auto v) const noexcept
  {
    atom_setfloat(at, v);
    return MAX_ERR_NONE;
  }

  t_max_err operator()(t_atom* at, std::string_view v) const noexcept
  {
    atom_setsym(at, gensym(v.data()));
    return MAX_ERR_NONE;
  }

  template <typename T>
    requires std::is_enum_v<T>
  t_max_err operator()(t_atom* at, T arg) noexcept
  {
    atom_setsym(at, gensym(magic_enum::enum_name(arg).data()));
    return MAX_ERR_NONE;
  }
};

struct to_atoms
{
  long* ac{};
  t_atom** av{};
  static std::span<t_atom> allocate(int n_atoms, long* ac, t_atom** av) noexcept
  {
    assert(ac);
    assert(av);
    if ((*ac >= n_atoms) && *av)
    {
      *ac = n_atoms;
    }
    else {
      *ac = n_atoms;
      *av = reinterpret_cast<t_atom *>(getbytes(sizeof(t_atom) * (*ac)));
      if (!(*av)) {
        *ac = 0;
        return {};
      }
    }

    return std::span<t_atom>(*av, *ac);
  }

  t_max_err operator()(std::integral auto v) const noexcept
  {
    if(auto atoms = allocate(1, ac, av); !atoms.empty())
    {
      atom_setlong(&atoms[0], v);
      return MAX_ERR_NONE;
    }
    return MAX_ERR_OUT_OF_MEM;
  }

  t_max_err operator()(std::floating_point auto v) const noexcept
  {
    if(auto atoms = allocate(1, ac, av); !atoms.empty())
    {
      atom_setfloat(&atoms[0], v);
      return MAX_ERR_NONE;
    }
    return MAX_ERR_OUT_OF_MEM;
  }

  t_max_err operator()(std::string_view v) const noexcept
  {
    if(auto atoms = allocate(1, ac, av); !atoms.empty())
    {
      atom_setsym(&atoms[0], gensym(v.data()));
      return MAX_ERR_NONE;
    }
    return MAX_ERR_OUT_OF_MEM;
  }

  template <typename T>
    requires std::is_enum_v<T>
  t_max_err operator()(T arg) noexcept
  {
    if(auto atoms = allocate(1, ac, av); !atoms.empty())
    {
      atom_setsym(&atoms[0], gensym(magic_enum::enum_name(arg).data()));
      return MAX_ERR_NONE;
    }
    return MAX_ERR_OUT_OF_MEM;
  }

  template <avnd::iterable_ish T>
  t_max_err operator()(const T& arg) noexcept
  {
    if(auto atoms = allocate(std::size(arg), ac, av); !atoms.empty())
    {
      using span_val_type = typename T::value_type;
      int i = 0;
      if constexpr(std::is_integral_v<span_val_type>)
      {
        for(auto& v : arg)
          atom_setlong(&atoms[i++], v);
      }
      else if constexpr(std::is_floating_point_v<span_val_type>)
      {
        for(auto& v : arg)
          atom_setfloat(&atoms[i], v);
      }
      else if constexpr(avnd::string_ish<span_val_type>)
      {
        for(auto& v : arg)
          atom_setsym(&atoms[i], gensym(v.data()));
      }
      else if constexpr(std::is_enum_v<span_val_type>)
      {
        for(auto& v : arg)
          atom_setsym(&atoms[i], gensym(magic_enum::enum_name(v).data()));
      }
      return MAX_ERR_NONE;
    }
    return MAX_ERR_OUT_OF_MEM;
  }
};
}
