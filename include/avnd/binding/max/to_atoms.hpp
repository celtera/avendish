#pragma once
#include <avnd/binding/max/helpers.hpp>
#include <avnd/concepts/field_names.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/common/for_nth.hpp>

#include <boost/container/vector.hpp>

#include <ext.h>

namespace max
{
struct to_list
{
  t_outlet* outlet{};
  static inline thread_local boost::container::vector<t_atom> atoms;

  to_list()
  {
    atoms.clear();
    atoms.reserve(256);
  }

  void operator()(std::floating_point auto arg)  noexcept
  {
    atom_setfloat(&atoms.emplace_back(), arg);
  }
  void operator()(std::integral auto arg) noexcept
  {
    atom_setlong(&atoms.emplace_back(), arg);
  }
  void operator()(std::string_view v)  noexcept
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

  template<typename F>
    requires std::is_aggregate_v<F>
  void operator()(const F& f)  noexcept
  {
    atoms.reserve(atoms.size() + boost::pfr::tuple_size_v<F>);

    avnd::for_each_field_ref(
        f, [this](const auto& v) mutable{
          (*this)(v);
        });
  }

  void operator()(const avnd::vector_ish auto& f)  noexcept
  {
    atoms.reserve(atoms.size() + f.size());
    for(auto& v : f) {
      (*this)(v);
    }
  }

  template<typename T, std::size_t N>
  void operator()(const std::array<T, N>& f)  noexcept
  {
    atoms.reserve(atoms.size() + f.size());
    for(auto& v : f) {
      (*this)(v);
    }
  }


  void operator()(const avnd::set_ish auto& f)  noexcept
  {
    atoms.reserve(atoms.size() + f.size());
    for(auto& v : f) {
      (*this)(v);
    }
  }

  void operator()(const avnd::span_ish auto& f)  noexcept
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

  template<avnd::tuple_ish U>
  void operator()(const U& f)  noexcept
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

struct aggregate_to_dict
{
  t_dictionary* d{};

  explicit aggregate_to_dict() noexcept
      : d{dictionary_new()}
  {

  }

  void operator()(t_symbol* k, std::floating_point auto v) noexcept
  {
    dictionary_appendfloat(d, k, v);
  }

  void operator()(t_symbol* k, std::integral auto v) noexcept
  {
    dictionary_appendlong(d, k, v);
  }

  void operator()(t_symbol* k, std::string_view v) noexcept
  {
    dictionary_appendstring(d, k, v.data());
  }

  template<typename T, std::size_t N>
  void operator()(t_symbol* k, std::array<T, N> v) noexcept
  {
  }

  template<typename F>
    requires avnd::has_field_names<F>
  void operator()(F&& f)
  {
    constexpr auto field_names = F::field_names();
    int k = 0;
    avnd::for_each_field_ref(
        f, [&](const auto& f) {
          constexpr auto name = field_names[k++];
          static const auto sym = gensym(name.data());
          add(sym, f);
        });
  }
};


struct to_atoms_large
{

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
      *av = (t_atom *)getbytes(sizeof(t_atom) * (*ac));
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
};
}
