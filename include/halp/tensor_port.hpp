#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

// halp::tensor_port<Name, Container, Kind = tensor_kind::generic>
//
// A port type whose value is an n-dimensional array. Any Container
// satisfying avnd::tensor_like (data + shape + optional strides) is
// accepted — typical choices:
//
//   halp::tensor_port<"signal", xt::xarray<double>>            signal;
//   halp::tensor_port<"frame",  Eigen::Tensor<float, 3>>       frame;
//   halp::tensor_port<"mat",    boost::multi_array<int, 2>>    mat;
//
// Marshalled to/from numpy in the Python backend via py::array_t
// (PEP 3118 buffer protocol; zero-copy when layout matches).
//
// The optional `Kind` template parameter is a semantic hint used by
// backends to:
//   - tag the lowered primitive with metadata (samplerate for spectra),
//   - render the tensor appropriately in a UI inspector
//     (waveform / table / heatmap / line plot),
//   - enforce static-rank invariants at compile time
//     (matrix → rank 2, vector → rank 1).
//
// The hint never changes the wire primitive (Pd typed-message / Max
// jit_matrix / TouchDesigner CHOP / numpy.ndarray): it travels as a
// description, not a different transport. Backends that don't know
// about a hint silently treat it as `generic`.

#include <avnd/concepts/tensor.hpp>
#include <halp/inline.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

namespace halp
{

// Read-only view into a tensor owned by the caller (typically the
// host's numpy array, Max jit_matrix, or TouchDesigner channel buffer).
// Use as the value type of a tensor_port to declare "I don't own the
// data, I just read it":
//
//   halp::tensor_port<"data", halp::tensor_view<double>> data;
//
// Saves the per-tick copy + allocation that an owning ndarray-style
// container would pay on input. The view is valid for the duration
// of the processor's operator() call; the backend binding guarantees
// the underlying buffer outlives that scope (via the `keep_alive`
// holder below).
//
// Backends that can't naturally provide a view (e.g. Pd, where data
// always arrives as an atom list) fall back to a buffered copy
// transparently — `keep_alive` then owns the temporary buffer.
template <typename T>
struct tensor_view
{
  using element_type = T;

  // Pointer into caller-owned memory; null when the input is unset.
  const T* data_ptr{nullptr};

  // Row-major shape and strides (in *elements*, not bytes). For
  // contiguous c-style buffers strides[d] = product of shape[d+1:].
  std::vector<std::size_t> shape_v;
  std::vector<std::size_t> strides_v;

  // Backend-specific keep-alive. Set by the binding when a value is
  // assigned; opaque to the port author. Replaced on every
  // `inst.field = ...` Python assignment (or equivalent on other
  // backends), so the previous view's buffer is released cleanly.
  std::shared_ptr<void> keep_alive;

  // Read API — mirrors the read side of the existing ndarray<T> shim
  const T* data() const noexcept { return data_ptr; }
  const std::vector<std::size_t>& shape() const noexcept { return shape_v; }
  const std::vector<std::size_t>& strides() const noexcept { return strides_v; }

  bool empty() const noexcept
  {
    if(data_ptr == nullptr || shape_v.empty())
      return true;
    for(auto e : shape_v)
      if(e == 0)
        return true;
    return false;
  }

  std::size_t total() const noexcept
  {
    if(data_ptr == nullptr)
      return 0;
    std::size_t t = 1;
    for(auto e : shape_v)
      t *= e;
    return t;
  }
};

// Trait so binding code can dispatch on tensor_view without including
// halp/tensor_port.hpp from the binding side.
template <typename V>
struct is_tensor_view : std::false_type {};
template <typename T>
struct is_tensor_view<tensor_view<T>> : std::true_type {};
template <typename V>
inline constexpr bool is_tensor_view_v = is_tensor_view<V>::value;

}  // namespace halp

namespace halp
{

// Semantic tag for tensor ports. Default is `generic` — no metadata,
// no rank constraint, no UI hint. Choose a more specific kind when
// the port's data has a known interpretation; backends use it to
// improve the user experience without changing the primitive on the
// wire.
//
// - `generic`     bare numeric tensor; backends pick a sensible default
// - `vector`      rank-1 assertion + UI renders as a line plot
// - `matrix`      rank-2 assertion + UI renders as a table / heatmap
// - `spectrum`    rank-1 frequency-domain magnitudes; samplerate
//                 metadata travels alongside if attached
// - `spectrogram` rank-2 time × freq; samplerate + window length
//                 metadata travels alongside if attached
enum class tensor_kind : unsigned char
{
  generic = 0,
  vector,
  matrix,
  spectrum,
  spectrogram,
};

constexpr std::size_t kind_to_rank(tensor_kind k) noexcept
{
  switch(k)
  {
    case tensor_kind::vector:
    case tensor_kind::spectrum:
      return 1;
    case tensor_kind::matrix:
    case tensor_kind::spectrogram:
      return 2;
    case tensor_kind::generic:
    default:
      return avnd::dynamic_rank;
  }
}

template <static_string Name, typename Container,
          tensor_kind Kind = tensor_kind::generic>
struct tensor_port
{
  using value_type = Container;
  static constexpr tensor_kind kind = Kind;
  static constexpr std::size_t static_rank
      = kind_to_rank(Kind) != avnd::dynamic_rank
            ? kind_to_rank(Kind)
            : avnd::container_static_rank<Container>();

  static clang_buggy_consteval auto name() noexcept
  {
    return std::string_view{Name.value};
  }

  Container value{};

  // Convenience accessors that go through the customization points.
  auto data() noexcept             { return avnd::data_of(value); }
  auto data() const noexcept       { return avnd::data_of(value); }
  auto shape() const noexcept      { return avnd::shape_of(value); }

  operator Container&() noexcept             { return value; }
  operator const Container&() const noexcept { return value; }
};

}  // namespace halp
