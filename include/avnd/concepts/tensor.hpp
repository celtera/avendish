#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

// Universal n-dimensional array (tensor) protocol for avendish.
//
// Designed to be a single concept reachable from every n-D array
// abstraction we want to interop with:
//
//   * Python-side:   numpy.ndarray, PyTorch / JAX / TensorFlow /
//                    CuPy / MLX tensors  (via DLPack / buffer protocol)
//   * C++-side:      std::mdspan, std::mdarray (C++26), xtensor::xarray,
//                    Eigen::Tensor, Eigen::Map<Matrix<…>>,
//                    boost::multi_array, Kokkos::View, OpenCV cv::Mat,
//                    nanobind::ndarray, raw user structs.
//   * Patcher-side:  Max/MSP jit.matrix, TouchDesigner CHOP/TOP/DAT
//                    (via the per-backend adapter, not this concept
//                    directly).
//
// The shape of every one of those converges on the same five fields:
//
//        { data_ptr, ndim, shape[ndim], strides[ndim] (opt), dtype }
//
// — with dtype carried either at compile time (mdspan, xtensor, Eigen)
// or at runtime (DLPack, numpy, jit.matrix, cv::Mat).
//
// Two layered concepts:
//
//   - `avnd::tensor_like<T>`           — element type known at compile
//                                        time; the user's port type T
//                                        carries Elem in its definition.
//   - `avnd::dynamic_tensor_like<T>`   — element type known at runtime;
//                                        T exposes a dtype() accessor.
//
// Conformance is checked via ADL-friendly free functions
// (avnd::data_of / shape_of / strides_of / resize_to), modelled after
// `std::ranges::size`. Users opt a custom type in by overloading one
// or more of those in their type's namespace.

#include <avnd/common/concepts_polyfill.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <type_traits>
#include <vector>

namespace avnd
{

// -----------------------------------------------------------------
// dtype_descriptor — DLPack-compatible.
//
// Layout is intentionally identical to nanobind::dlpack::dtype and to
// DLPack's DLDataType (DLPack 0.7+) so that values are bit-for-bit
// interconvertible. Backends that talk to either protocol can
// reinterpret_cast.
// -----------------------------------------------------------------

enum class dtype_code : uint8_t
{
  Int = 0,
  UInt = 1,
  Float = 2,
  OpaqueHandle = 3,
  Bfloat = 4,
  Complex = 5,
  Bool = 6,
};

struct dtype_descriptor
{
  uint8_t code{};      // value of dtype_code
  uint8_t bits{};      // 1, 8, 16, 32, 64, 128
  uint16_t lanes{1};   // per-cell SIMD lanes (Max jit "planes" = lanes)

  constexpr bool operator==(const dtype_descriptor&) const = default;

  static constexpr dtype_descriptor make(dtype_code c, uint8_t b, uint16_t l = 1) noexcept
  {
    return {static_cast<uint8_t>(c), b, l};
  }
};

// dtype_of<T> — compile-time element type → DLPack descriptor.

namespace detail
{
template <typename T>
struct dtype_of_helper;

template <>
struct dtype_of_helper<bool>
{
  static constexpr dtype_descriptor value = dtype_descriptor::make(dtype_code::Bool, 8);
};

#define AVND_DEF_DTYPE(TYPE, CODE, BITS)                            \
  template <>                                                       \
  struct dtype_of_helper<TYPE>                                      \
  {                                                                 \
    static constexpr dtype_descriptor value                         \
        = dtype_descriptor::make(dtype_code::CODE, BITS);           \
  };

AVND_DEF_DTYPE(int8_t,   Int,  8)
AVND_DEF_DTYPE(int16_t,  Int,  16)
AVND_DEF_DTYPE(int32_t,  Int,  32)
AVND_DEF_DTYPE(int64_t,  Int,  64)
AVND_DEF_DTYPE(uint8_t,  UInt, 8)
AVND_DEF_DTYPE(uint16_t, UInt, 16)
AVND_DEF_DTYPE(uint32_t, UInt, 32)
AVND_DEF_DTYPE(uint64_t, UInt, 64)
AVND_DEF_DTYPE(float,    Float, 32)
AVND_DEF_DTYPE(double,   Float, 64)
#undef AVND_DEF_DTYPE
}  // namespace detail

template <typename T>
inline constexpr dtype_descriptor dtype_of
    = detail::dtype_of_helper<std::remove_cv_t<T>>::value;

template <typename T>
concept has_avnd_dtype = requires { detail::dtype_of_helper<std::remove_cv_t<T>>::value; };

// -----------------------------------------------------------------
// Customization points: free functions providing the tensor accessors.
//
// Default implementations look for member functions of the same name.
// Users adapt non-conforming types (e.g. Eigen::Tensor uses
// .dimensions() instead of .shape()) by overloading these in the
// type's own namespace; ADL picks them up.
// -----------------------------------------------------------------

template <typename T>
constexpr auto data_of(T&& t) noexcept(noexcept(t.data()))
    -> decltype(t.data())
{
  return t.data();
}

template <typename T>
constexpr auto shape_of(const T& t) noexcept(noexcept(t.shape()))
    -> decltype(t.shape())
{
  return t.shape();
}

template <typename T>
constexpr auto strides_of(const T& t) noexcept(noexcept(t.strides()))
    -> decltype(t.strides())
{
  return t.strides();
}

template <typename T, typename Shape>
constexpr void resize_to(T& t, Shape&& shape) noexcept(noexcept(t.resize(shape)))
{
  t.resize(std::forward<Shape>(shape));
}

// -----------------------------------------------------------------
// Concept: tensor_like<T>
//
// A T is a tensor when:
//   - avnd::data_of(t)  → pointer to the element type
//   - avnd::shape_of(t) → a sized range of integral extents
//
// Element type is deduced from data_of's return; no `value_type`
// typedef required (handles types whose value_type is a row, not the
// scalar, e.g. boost::multi_array).
// -----------------------------------------------------------------

namespace detail
{
template <typename T>
using data_result_t = decltype(avnd::data_of(std::declval<T&>()));

template <typename T>
using shape_result_t = decltype(avnd::shape_of(std::declval<const T&>()));
}  // namespace detail

template <typename T>
concept tensor_like = requires(T& t, const T& ct) {
  avnd::data_of(t);
  avnd::shape_of(ct);
  requires std::is_pointer_v<detail::data_result_t<T>>;
  requires std::ranges::sized_range<detail::shape_result_t<T>>;
};

// tensor_element<T> — element type of a tensor (T's data_of returns Elem* or const Elem*).

template <tensor_like T>
using tensor_element
    = std::remove_cv_t<std::remove_pointer_t<detail::data_result_t<T>>>;

// Optional protocol members.

template <typename T>
concept has_tensor_strides = tensor_like<T> && requires(const T& ct) {
  avnd::strides_of(ct);
  requires std::ranges::sized_range<decltype(avnd::strides_of(ct))>;
};

template <typename T>
concept resizable_tensor_like
    = tensor_like<T> && requires(T t, std::vector<std::size_t> sh) {
        avnd::resize_to(t, sh);
      };

// Mutable view: data_of yields a non-const pointer.
template <typename T>
concept mutable_tensor_like
    = tensor_like<T> && !std::is_const_v<std::remove_pointer_t<detail::data_result_t<T>>>;

// -----------------------------------------------------------------
// Concept: dynamic_tensor_like<T>
//
// Type-erased tensors: DLPack's DLTensor, OpenCV cv::Mat, a numpy
// ndarray exposed as a buffer view, Max jit_matrix_info. These carry
// a runtime dtype descriptor alongside the data pointer.
// -----------------------------------------------------------------

template <typename T>
concept dynamic_tensor_like = requires(const T& ct) {
  { avnd::data_of(ct) } -> std::convertible_to<const void*>;
  { ct.dtype() } -> std::convertible_to<dtype_descriptor>;
  avnd::shape_of(ct);
  { ct.ndim() } -> std::convertible_to<std::size_t>;
};

// -----------------------------------------------------------------
// rank_of: compile-time rank if the shape range has fixed size,
// otherwise std::dynamic_extent.
// -----------------------------------------------------------------

inline constexpr std::size_t dynamic_rank = static_cast<std::size_t>(-1);

template <tensor_like T>
constexpr std::size_t rank_of(const T& t) noexcept
{
  return std::ranges::size(avnd::shape_of(t));
}

// Compile-time rank, picked up from common conventions
// (Eigen::Tensor::NumDimensions, boost::multi_array::dimensionality,
// xtensor::rank). Returns dynamic_rank otherwise.
template <typename T>
constexpr std::size_t container_static_rank() noexcept
{
  using C = std::remove_cvref_t<T>;
  if constexpr(requires { { C::NumDimensions } -> std::convertible_to<std::size_t>; })
    return static_cast<std::size_t>(C::NumDimensions);
  else if constexpr(requires { { C::dimensionality } -> std::convertible_to<std::size_t>; })
    return static_cast<std::size_t>(C::dimensionality);
  else if constexpr(requires { { C::rank } -> std::convertible_to<std::size_t>; })
    return static_cast<std::size_t>(C::rank);
  else
    return dynamic_rank;
}

template <typename Field>
concept tensor_port
    = requires { typename std::remove_cvref_t<decltype(Field::value)>; }
      && tensor_like<std::remove_cvref_t<decltype(Field::value)>>;

}  // namespace avnd
