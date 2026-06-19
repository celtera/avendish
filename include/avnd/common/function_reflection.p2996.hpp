#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// P2996 backend for avnd::function_reflection<auto f>: recovers the function
// declaration from the pointer and queries it through std::meta.
// Primary template is in function_reflection.hpp.

#include <avnd/common/meta_polyfill.hpp>

#include <boost/mp11/list.hpp>

#include <type_traits>
#include <vector>

namespace avnd
{
namespace detail
{
// Class of a pointer-to-member T C::*.
template <typename M>
struct member_ptr_class;
template <typename T, typename C>
struct member_ptr_class<T C::*>
{
  using type = C;
};

// Recover the (member-)function declaration reflection from the pointer f.
template <auto f>
consteval std::meta::info function_decl()
{
  using PT = std::remove_cv_t<std::remove_reference_t<decltype(f)>>;
  if constexpr(std::is_member_function_pointer_v<PT>)
  {
    using C = typename member_ptr_class<PT>::type;
    std::meta::info found{};
    template for(constexpr std::meta::info m : std::define_static_array(
                     std::meta::members_of(^^C, std::meta::access_context::unchecked())))
    {
      if constexpr(std::meta::is_function(m)
                   && !std::meta::is_special_member_function(m))
        // Only same-typed overloads are comparable.
        if constexpr(std::is_same_v<std::remove_cv_t<decltype(&[:m:])>, PT>)
          if(&[:m:] == f)
            found = m;
    }
    return found;
  }
  else
  {
    return std::meta::reflect_function(*f);
  }
}

consteval std::vector<std::meta::info> param_type_infos(std::meta::info d)
{
  std::vector<std::meta::info> v;
  for(std::meta::info p : std::meta::parameters_of(d))
    v.push_back(std::meta::type_of(p));
  return v;
}

// Carries class_type only for member functions.
template <std::meta::info D, bool IsMember>
struct fn_class_carrier
{
};
template <std::meta::info D>
struct fn_class_carrier<D, true>
{
  using class_type = [:std::meta::parent_of(D):];
};
}

template <auto f>
struct function_reflection
    : detail::fn_class_carrier<
          detail::function_decl<f>(),
          std::is_member_function_pointer_v<
              std::remove_cv_t<std::remove_reference_t<decltype(f)>>>>
{
  static constexpr std::meta::info decl = detail::function_decl<f>();

  using arguments =
      [:std::meta::substitute(^^boost::mp11::mp_list, detail::param_type_infos(decl)):];
  using return_type = [:std::meta::return_type_of(decl):];

  static constexpr auto count = std::meta::parameters_of(decl).size();
  static constexpr bool is_const = std::meta::is_const(decl);
  static constexpr bool is_volatile = std::meta::is_volatile(decl);
  static constexpr bool is_noexcept = std::meta::is_noexcept(decl);
  static constexpr bool is_reference = std::meta::is_lvalue_reference_qualified(decl);
  static constexpr bool is_rvalue_reference
      = std::meta::is_rvalue_reference_qualified(decl);
};
}
