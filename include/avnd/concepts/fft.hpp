#pragma once
#include <avnd/common/concepts_polyfill.hpp>

namespace avnd
{

template<typename T>
concept complex_number =
        std::is_same_v<std::remove_cvref_t<T>, float[2]>
     || std::is_same_v<std::remove_cvref_t<T>, double[2]>
     || requires (T t) { t.real(); t.imag(); }
;

// Forward FFT
template <typename FP, typename T>
concept fft_1d = requires(T t)
{
  // How our FFT operates
  std::is_same_v<FP, typename T::real_type>;
  typename T::complex_type;

  // Initializes the internal data structures if any
  t.reset(128);

  // Processes the 1D fft on a real buffer yields complex numbers
  { t.execute(std::add_pointer_t<typename T::real_type>{}, std::size_t(0)) } -> std::convertible_to<typename T::complex_type*>;

  // Which may need a normalization step
  { t.normalization(128) } -> std::floating_point;
};

// Backwards fft
template <typename FP, typename T>
concept rfft_1d = requires(T t)
{
  // How our FFT operates
  std::is_same_v<FP, typename T::real_type>;
  typename T::complex_type;

  // Initializes the internal data structures if any
  t.reset(128);

  // Processes the 1D fft on a complex buffer yields real numbers
  { t.execute(std::add_pointer_t<typename T::complex_type>{}, std::size_t(0)) } -> std::convertible_to<typename T::real_type*>;

  // Which may need a normalization step
  { t.normalization(128) } -> std::floating_point;
};

// TODO spectra input port
}
