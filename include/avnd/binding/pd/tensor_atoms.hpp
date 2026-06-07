#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Two cooperating wire formats for tensor ports:
//   bare list   [name v0 v1 ... vN-1)       — rank-1
//   typed msg   [name tensor ndim d0 ... vN) — any rank
// Readers accept both; element conversion goes through t_float.

#include <avnd/binding/pd/helpers.hpp>
#include <avnd/concepts/tensor.hpp>
#include <halp/tensor_port.hpp>
#include <m_pd.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace pd
{

inline t_symbol* tensor_sym() noexcept
{
  static t_symbol* sym = gensym("tensor");
  return sym;
}

template <typename T>
  requires std::is_arithmetic_v<T>
inline T tensor_element_from_atom(const t_atom& av) noexcept
{
  if(av.a_type == A_FLOAT)
    return static_cast<T>(av.a_w.w_float);
  return T{};
}

template <typename T>
  requires std::is_arithmetic_v<T>
inline void tensor_element_to_atom(t_atom& atom, T v) noexcept
{
  atom = {.a_type = A_FLOAT, .a_w = {.w_float = static_cast<t_float>(v)}};
}

template <typename Container>
  requires avnd::tensor_like<Container>
inline bool apply_atoms_to_tensor(Container& container, int argc, t_atom* argv) noexcept
{
  using element_type = avnd::tensor_element<Container>;
  static_assert(std::is_arithmetic_v<element_type>,
                "Pd tensor wire only supports arithmetic element types");

  if(argc <= 0)
    return false;

  const bool is_typed = (argv[0].a_type == A_SYMBOL)
                        && (argv[0].a_w.w_symbol == tensor_sym());

  std::vector<std::size_t> shape;
  int payload_offset = 0;
  std::size_t total = 0;

  if(is_typed)
  {
    if(argc < 2 || argv[1].a_type != A_FLOAT)
      return false;
    const int ndim = static_cast<int>(argv[1].a_w.w_float);
    if(ndim < 0)
      return false;
    if(argc < 2 + ndim)
      return false;

    shape.resize(static_cast<std::size_t>(ndim));
    total = 1u;
    for(int d = 0; d < ndim; ++d)
    {
      const auto& a = argv[2 + d];
      if(a.a_type != A_FLOAT)
        return false;
      const auto v = a.a_w.w_float;
      if(v < 0.f)
        return false;
      const std::size_t dv = static_cast<std::size_t>(v);
      shape[static_cast<std::size_t>(d)] = dv;
      total *= dv;
    }

    payload_offset = 2 + ndim;
    if(static_cast<std::size_t>(argc - payload_offset) != total)
      return false;
  }
  else
  {
    shape = {static_cast<std::size_t>(argc)};
    total = static_cast<std::size_t>(argc);
    payload_offset = 0;
  }

  if constexpr(halp::is_tensor_view_v<Container>)
  {
    auto buf = std::make_shared<std::vector<element_type>>(total);
    auto* dst = buf->data();
    for(std::size_t i = 0; i < total; ++i)
    {
      dst[i] = tensor_element_from_atom<element_type>(argv[payload_offset + static_cast<int>(i)]);
    }

    // Row-major contiguous strides (in elements).
    std::vector<std::size_t> strides(shape.size(), 1);
    for(std::ptrdiff_t i = static_cast<std::ptrdiff_t>(shape.size()) - 2; i >= 0; --i)
    {
      strides[static_cast<std::size_t>(i)]
          = strides[static_cast<std::size_t>(i) + 1] * shape[static_cast<std::size_t>(i) + 1];
    }

    container.data_ptr = dst;
    container.shape_v = std::move(shape);
    container.strides_v = std::move(strides);
    container.keep_alive
        = std::shared_ptr<void>(buf, static_cast<void*>(buf->data()));
    return true;
  }
  else if constexpr(avnd::resizable_tensor_like<Container>)
  {
    avnd::resize_to(container, shape);
    auto* dst = avnd::data_of(container);
    if(dst == nullptr && total > 0)
      return false;
    for(std::size_t i = 0; i < total; ++i)
    {
      dst[i] = tensor_element_from_atom<element_type>(argv[payload_offset + static_cast<int>(i)]);
    }
    return true;
  }
  else
  {
    auto* dst = avnd::data_of(container);
    if(dst == nullptr)
      return false;
    std::size_t fixed_total = 1;
    for(auto e : avnd::shape_of(container))
      fixed_total *= e;
    if(fixed_total != total)
      return false;
    for(std::size_t i = 0; i < total; ++i)
    {
      dst[i] = tensor_element_from_atom<element_type>(argv[payload_offset + static_cast<int>(i)]);
    }
    return true;
  }
}

template <std::size_t StaticRank, typename Container>
  requires avnd::tensor_like<Container>
inline void emit_tensor(t_outlet* outlet, const Container& container) noexcept
{
  using element_type = avnd::tensor_element<Container>;
  static_assert(std::is_arithmetic_v<element_type>,
                "Pd tensor wire only supports arithmetic element types");

  auto* src = avnd::data_of(container);
  const auto& sh = avnd::shape_of(container);

  std::size_t total = 1;
  std::size_t ndim_runtime = 0;
  for(auto e : sh)
  {
    total *= static_cast<std::size_t>(e);
    ++ndim_runtime;
  }

  if(src == nullptr || total == 0)
  {
    if constexpr(StaticRank == 1)
    {
      outlet_list(outlet, &s_list, 0, nullptr);
    }
    else
    {
      static thread_local std::vector<t_atom> atoms;
      atoms.clear();
      atoms.resize(1 + ndim_runtime);
      tensor_element_to_atom(atoms[0], static_cast<float>(ndim_runtime));
      std::size_t i = 1;
      for(auto e : sh)
        tensor_element_to_atom(atoms[i++], static_cast<float>(e));
      outlet_anything(outlet, tensor_sym(),
                      static_cast<int>(atoms.size()), atoms.data());
    }
    return;
  }

  if constexpr(StaticRank == 1)
  {
    static thread_local std::vector<t_atom> atoms;
    atoms.clear();
    atoms.resize(total);
    for(std::size_t i = 0; i < total; ++i)
      tensor_element_to_atom(atoms[i], src[i]);
    outlet_list(outlet, &s_list, static_cast<int>(total), atoms.data());
  }
  else
  {
    static thread_local std::vector<t_atom> atoms;
    atoms.clear();
    atoms.resize(1 + ndim_runtime + total);

    tensor_element_to_atom(atoms[0], static_cast<float>(ndim_runtime));
    std::size_t k = 1;
    for(auto e : sh)
      tensor_element_to_atom(atoms[k++], static_cast<float>(e));
    for(std::size_t i = 0; i < total; ++i)
      tensor_element_to_atom(atoms[k++], src[i]);

    outlet_anything(outlet, tensor_sym(),
                    static_cast<int>(atoms.size()), atoms.data());
  }
}

}  // namespace pd
