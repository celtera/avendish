#pragma once
#include <avnd/concepts/fft.hpp>

#include <vector>
#include <complex>
#include <cmath>

namespace halp
{
namespace detail
{
// Minimal implementation taken from https://gist.github.com/lukicdarkoo/3f0d056e9244784f8b4a
// Instead go use fftw, MKL, KFR or whatever !
template<typename FP>
static void fft_rec(std::complex<FP> *x, int N)
{
  using namespace std;
  using cplx = std::complex<FP>;
  static constexpr double pi = 3.141592653589793238462643383279502884;

  // Check if it is splitted enough
  if (N <= 1) {
    return;
  }

  // Split even and odd
  auto* odd = (cplx*)alloca(sizeof(cplx) * (1 + N/2));
  auto* even = (cplx*)alloca(sizeof(cplx) * (1 + N/2));
  for (int i = 0; i < N / 2; i++) {
    even[i] = x[i*2];
    odd[i] = x[i*2+1];
  }

  // Split on tasks
  fft_rec(even, N/2);
  fft_rec(odd, N/2);

  // Calculate DFT
  for (int k = 0; k < N / 2; k++) {
    cplx t = std::exp(cplx(0, -2 * pi * k / N)) * odd[k];
    x[k] = even[k] + t;
    x[N / 2 + k] = even[k] - t;
  }
}
}

template<typename FP>
class fft
{
public:
  template<typename T>
  using fft_type = fft<T>;

  using real_type = FP;
  using complex_type = std::complex<FP>;

  constexpr double normalization(std::size_t N)
  {
     return 1. / N;
  }

  void reset(std::size_t N)
  {
    m_cplx.resize(N + 1);
    m_real.resize(N + 1);
  }

  // Real to complex
  complex_type* execute(real_type* x_in, std::size_t N)
  {
    if(m_cplx.size() != N + 1) {
      m_cplx.resize(N + 1);
    }

    auto x_out = m_cplx.data();
    for (int i = 0; i < N; i++) {
      x_out[i] = {x_in[i], 0};
    }

    detail::fft_rec(x_out, N);

    return x_out;
  }

  // Complex to real
  real_type* execute(complex_type* x_in, std::size_t N)
  {
    if(m_cplx.size() != N + 1) {
      m_cplx.resize(N + 1);
    }
    if(m_real.size() != N + 1) {
      m_real.resize(N + 1);
    }

    auto cplx = m_cplx.data();
    for (int i = 0; i < N; i++) {
      cplx[i] = {x_in[i].imag(), x_in[i].real()};
    }

    detail::fft_rec(cplx, N);

    auto x_out = m_real.data();
    for (int i = 0; i < N; i++) {
        x_out[i] = cplx[i].imag();
    }

    return x_out;
  }

private:
  std::vector<std::complex<FP>> m_cplx;
  std::vector<FP> m_real;
};

template <typename C, typename FP>
concept has_fft_1d = avnd::fft_1d<FP, typename C::template fft_type<FP>>;
}
static_assert(avnd::fft_1d<float, halp::fft<float>>);
static_assert(avnd::fft_1d<double, halp::fft<double>>);
static_assert(halp::has_fft_1d<halp::fft<float>, float>);
static_assert(halp::has_fft_1d<halp::fft<double>, double>);
