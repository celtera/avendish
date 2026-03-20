#pragma once
#include <avnd/common/struct_reflection.hpp>
#include <avnd/introspection/port.hpp>

#define generate_predicate_introspection(Concept)                \
  template <typename Field>                                      \
  using is_##Concept##_t = boost::mp11::mp_bool<Concept<Field>>; \
                                                                 \
  template <typename T>                                          \
  using Concept##_introspection = avnd::predicate_introspection<T, is_##Concept##_t>;

#define generate_member_introspection(Concept, Member)           \
  template <typename T>                                          \
  struct Concept##_##Member##_introspection                      \
      : Concept##_introspection<typename avnd::Member##_type<T>::type> \
  {                                                              \
  };
