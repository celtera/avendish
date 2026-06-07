#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

// Customization-point overloads that adapt n-D array libraries which
// don't expose data()/shape()/strides() under those exact names.
//
// Include this header only in TUs that already include the relevant
// library (xtensor, Eigen, boost::multi_array, …). Each adapter
// section is wrapped in __has_include so the avendish core stays
// dependency-free for users who only need one or two of them.

#include <avnd/concepts/tensor.hpp>

#include <array>
#include <cstddef>

// -----------------------------------------------------------------
// xtensor — xt::xarray<T>, xt::xtensor<T, N>, xt::xadapt(...)
// All already conform via data() / shape() / strides() member fns,
// so no overload needed. Listed here for documentation only.
// -----------------------------------------------------------------

// -----------------------------------------------------------------
// std::mdspan (or Kokkos::mdspan pre-C++23)
//
// mdspan exposes data_handle() instead of data(), and extents() instead
// of shape(). Strides depend on the LayoutPolicy.
// -----------------------------------------------------------------

#if __has_include(<mdspan>)
#include <mdspan>
namespace std
{
template <typename T, typename Ext, typename L, typename A>
constexpr auto data_of(mdspan<T, Ext, L, A>& m) noexcept
{
  return m.data_handle();
}
template <typename T, typename Ext, typename L, typename A>
constexpr auto data_of(const mdspan<T, Ext, L, A>& m) noexcept
{
  return m.data_handle();
}

namespace mdspan_shim_detail
{
template <typename Ext>
constexpr auto extents_to_array(const Ext& e) noexcept
{
  std::array<std::size_t, Ext::rank()> out{};
  for(std::size_t i = 0; i < Ext::rank(); ++i)
    out[i] = e.extent(i);
  return out;
}
}  // namespace mdspan_shim_detail

template <typename T, typename Ext, typename L, typename A>
constexpr auto shape_of(const mdspan<T, Ext, L, A>& m) noexcept
{
  return mdspan_shim_detail::extents_to_array(m.extents());
}

template <typename T, typename Ext, typename L, typename A>
constexpr auto strides_of(const mdspan<T, Ext, L, A>& m) noexcept
{
  std::array<std::size_t, Ext::rank()> out{};
  for(std::size_t i = 0; i < Ext::rank(); ++i)
    out[i] = m.stride(i);
  return out;
}
}  // namespace std
#endif

// -----------------------------------------------------------------
// Eigen::Tensor (unsupported module)
//
// Uses .data() (ok) but .dimensions() instead of .shape().
// -----------------------------------------------------------------

#if __has_include(<unsupported/Eigen/CXX11/Tensor>)
#include <unsupported/Eigen/CXX11/Tensor>
namespace Eigen
{
template <typename T, int N, int Opts, typename Idx>
constexpr auto shape_of(const Tensor<T, N, Opts, Idx>& t) noexcept
{
  std::array<std::size_t, static_cast<std::size_t>(N)> out{};
  const auto& d = t.dimensions();
  for(std::size_t i = 0; i < static_cast<std::size_t>(N); ++i)
    out[i] = static_cast<std::size_t>(d[i]);
  return out;
}

template <typename T, int N, int Opts, typename Idx>
constexpr auto strides_of(const Tensor<T, N, Opts, Idx>& t) noexcept
{
  // Eigen::Tensor is column-major by default (RowMajor opt-in).
  // Compute strides from dimensions according to the storage order.
  std::array<std::size_t, static_cast<std::size_t>(N)> out{};
  if constexpr((Opts & Eigen::RowMajor) != 0)
  {
    std::size_t s = 1;
    for(std::ptrdiff_t i = N - 1; i >= 0; --i)
    {
      out[static_cast<std::size_t>(i)] = s;
      s *= static_cast<std::size_t>(t.dimension(i));
    }
  }
  else
  {
    std::size_t s = 1;
    for(int i = 0; i < N; ++i)
    {
      out[static_cast<std::size_t>(i)] = s;
      s *= static_cast<std::size_t>(t.dimension(i));
    }
  }
  return out;
}
}  // namespace Eigen
#endif

// -----------------------------------------------------------------
// boost::multi_array<T, N>
//
// .data() is ok. .shape() returns const size_type*, not a range, so we
// adapt it to a std::array via the known rank.
// -----------------------------------------------------------------

#if __has_include(<boost/multi_array.hpp>)
#include <boost/multi_array.hpp>
namespace boost
{
template <typename T, std::size_t N, typename A>
constexpr auto shape_of(const multi_array<T, N, A>& a) noexcept
{
  std::array<std::size_t, N> out{};
  const auto* s = a.shape();
  for(std::size_t i = 0; i < N; ++i)
    out[i] = s[i];
  return out;
}

template <typename T, std::size_t N, typename A>
constexpr auto strides_of(const multi_array<T, N, A>& a) noexcept
{
  std::array<std::size_t, N> out{};
  const auto* s = a.strides();
  for(std::size_t i = 0; i < N; ++i)
    out[i] = static_cast<std::size_t>(s[i]);
  return out;
}
}  // namespace boost
#endif

// -----------------------------------------------------------------
// OpenCV cv::Mat — runtime-typed; goes through dynamic_tensor_like.
// -----------------------------------------------------------------

#if __has_include(<opencv2/core/mat.hpp>)
#include <opencv2/core/mat.hpp>
namespace cv
{
constexpr auto data_of(const Mat& m) noexcept -> const void*
{
  return m.data;
}
constexpr auto data_of(Mat& m) noexcept -> void* { return m.data; }

inline std::vector<std::size_t> shape_of(const Mat& m)
{
  std::vector<std::size_t> out(static_cast<std::size_t>(m.dims));
  for(int i = 0; i < m.dims; ++i)
    out[static_cast<std::size_t>(i)] = static_cast<std::size_t>(m.size[i]);
  return out;
}

inline std::vector<std::size_t> strides_of(const Mat& m)
{
  std::vector<std::size_t> out(static_cast<std::size_t>(m.dims));
  for(int i = 0; i < m.dims; ++i)
    out[static_cast<std::size_t>(i)] = static_cast<std::size_t>(m.step[i]);
  return out;
}
}  // namespace cv
#endif
