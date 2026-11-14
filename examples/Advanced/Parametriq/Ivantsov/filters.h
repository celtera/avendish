/* Yuriy Ivantsov's ivantsov-filters, under the MIT license in the LICENSE
 * file next to this one, from https://github.com/yIvantsov/ivantsov-filters
 * at commit f3dec122c8b04988cfb1a5a247006572e11d7793 ("Support configurable
 * filter orders and simplify the API"), with three changes that are not
 * upstream:
 * - the "performance-v2" revision (ivantsov-filters-performance-v2, its
 *   ivantsov-filters-v2-from-upstream.patch applied to that commit), which
 *   reworks the coefficient computation and processing for speed. The
 *   resulting header has SHA-256
 *   cf88a2e56dde8a5b40e5ccb4861f2fae595e0dca85d1ef66e562ccfd13f4e407;
 * - Filter::Response::complex_response_hz, and the <complex> include it
 *   needs, which give the phase of a response so that Parametriq's
 *   oversampled analytic curve can sum the resampler's images;
 * - sample_type is a partially specialised trait instead of a lambda in an
 *   unevaluated context, which GCC 14.2 cannot substitute (internal compiler
 *   error in tsubst).
 */
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <limits>
#include <numbers>
#include <type_traits>
#include <utility>

namespace ivantsov {
template<typename Sample>
struct sample_type_of : std::type_identity<Sample> {};
template<typename Sample>
     requires requires { typename Sample::value_type; }
struct sample_type_of<Sample> : std::type_identity<typename Sample::value_type> {};
template<typename Sample>
using sample_type = typename sample_type_of<Sample>::type;

enum struct Correction { None, Omega, Sigma };

namespace Linear {
enum struct Type { HighPass, LowPass, AllPass, HighShelf, LowShelf, BandPass, BandStop, BandShelf };
enum struct Param { Fc, Sigma, Gain, Zeta };

using enum Type;
using enum Param;

template<auto Type>
concept is_band = Type == BandPass || Type == BandStop || Type == BandShelf;

template<auto Type>
concept is_shelf = Type == HighShelf || Type == LowShelf || Type == BandShelf;

// State defaults to Sample. Scalar float I/O can opt into double coefficients
// and states by specifying double as the fifth template argument.
template<auto Type, auto Order, typename Sample, auto Correction = Correction::None, typename State = Sample>
     requires std::floating_point<sample_type<Sample>> && std::floating_point<sample_type<State>> &&
              (Order > 0) && (!is_band<Type> || Order % 2 == 0)
struct Filter {
     using T = sample_type<State>;

 private:
     using W = std::common_type_t<T, double>;
     static constexpr auto count = (Order + 1) / 2;
     using Coefficients = std::array<std::array<T, 4>, count>;
     using Responses = std::array<std::array<W, 6>, count>;

 public:
     // Build once for a response plot, then query many frequencies. This
     // immutable snapshot remains valid when the filter is changed later.
     class Response {
          friend struct Filter;
          T fs {};
          Responses coefficients {};

      public:
          template<auto N = 0>
          auto magnitude(const T cosine) const -> T {
               if (!positive(fs) || !std::isfinite(cosine) || cosine < T {-1} || cosine > T {1}) {
                    return nan();
               }
               return static_cast<T>(response_at<N>(std::sqrt((W {1} + cosine) / W {2}),
                                                    std::sqrt((W {1} - cosine) / W {2}), coefficients));
          }

          auto magnitude_hz(const T frequency) const -> T {
               if (!positive(fs) || !std::isfinite(frequency) || frequency < T {} || frequency > fs / T {2}) {
                    return nan();
               }
               const W angle {std::numbers::pi_v<W> * (W(frequency) / W(fs))};
               const W c {frequency == fs / T {2} ? W {} : std::cos(angle)};
               return static_cast<T>(response_at<0>(c, std::sin(angle), coefficients));
          }

          // Local Parametriq addition: preserve phase for exact multirate
          // image/alias summation. Negative frequencies return the conjugate.
          auto complex_response_hz(const T frequency) const -> std::complex<T> {
               if (!positive(fs) || !std::isfinite(frequency) || std::abs(frequency) > fs / T {2}) {
                    return {nan(), nan()};
               }
               const W angle {std::numbers::pi_v<W> * (W(frequency) / W(fs))};
               const W c {std::abs(frequency) == fs / T {2} ? W {} : std::cos(angle)};
               const W s {std::sin(angle)};
               std::complex<W> result {1, 0};
               for (int n = 0; n < count; ++n) {
                    const auto& r = coefficients[n];
                    if (n == 0 && Order % 2 != 0) {
                         result *= std::complex<W> {r[0] * c, r[1] * s}
                                   / std::complex<W> {r[3] * c, r[4] * s};
                    } else {
                         result *= std::complex<W> {r[0] * c * c - r[2] * s * s, r[1] * s * c}
                                   / std::complex<W> {r[3] * c * c - r[5] * s * s, r[4] * s * c};
                    }
               }
               return {static_cast<T>(result.real()), static_cast<T>(result.imag())};
          }
     };

     // Successful reinitialisation retains the requested cutoff and parameters,
     // rebuilds the coefficients for the new rate, and clears the state.
     auto initialise(const T x) -> bool {
          if (!positive(x)) { return false; }
          auto next {parameters};
          next.fs = x;
          next.pi_over_fs = std::numbers::pi_v<W> / W(x);
          if (!next.has_cutoff) { next.fc = std::min(T {1000}, x / T {4}); }
          if (!prepare_cutoff(next) || !commit(next)) { return false; }
          reset();
          return true;
     }

     template<auto N = 0>
     auto processed(const Sample x) -> Sample {
          return static_cast<Sample>(process<N>(static_cast<State>(x)));
     }

     // Coefficients stay fixed for this block. The local copy lets the compiler
     // keep recursive state in registers even when Sample and State have the
     // same scalar type. Input and output must be disjoint or the same buffer;
     // neither may overlap this Filter object's storage.
     auto process_block(const Sample* input, Sample* output, const std::size_t frames) -> void {
          if (frames == 0) { return; }
          auto local {*this};
          for (std::size_t n = 0; n < frames; ++n) { output[n] = local.processed(input[n]); }
          z = local.z;
     }

     auto reset() -> void {
          z.fill({State {T {}}, State {T {}}});
     }

     // Invalid updates leave both the coefficients and state unchanged. There
     // are no exceptions or allocations on the control or audio paths.
     template<auto... Param>
     auto set(const decltype(Param, T {})... x) -> bool
          requires(... && (Param == Fc || (Param == Sigma && Correction == Correction::None) ||
                           (Param == Gain && is_shelf<Type>) || (Param == Zeta && Order > 1))) {
          if (!(parameters.fs > T {})) { return false; }
          auto next {parameters};
          bool valid {true};
          bool cutoff_changed {false};
          (..., [&] {
               if constexpr (Param == Fc) {
                    valid = valid && positive(x);
                    next.fc = x;
                    next.has_cutoff = true;
                    cutoff_changed = true;
               }
               if constexpr (Param == Sigma) {
                    valid = valid && std::isfinite(x) && x >= T {};
                    next.sigma = x;
               }
               if constexpr (Param == Gain) {
                    valid = valid && positive(x);
                    if (positive(x)) { next.gain = gain_root(x); }
               }
               if constexpr (Param == Zeta) {
                    valid = valid && positive(x);
                    next.zeta = x;
               }
          }());
          if (!valid || (cutoff_changed && !prepare_cutoff(next))) { return false; }
          return commit(next);
     }

     // Compatibility API: x is cos(2*pi*f/fs). A float cosine already rounded
     // to 1 cannot represent a low nonzero frequency; prefer magnitude_hz.
     template<auto N = 0>
     auto magnitude(const T x) const -> T {
          if (!positive(parameters.fs) || !std::isfinite(x) || x < T {-1} || x > T {1}) { return nan(); }
          return response().template magnitude<N>(x);
     }

     auto magnitude_hz(const T frequency) const -> T {
          if (!positive(parameters.fs) || !std::isfinite(frequency) || frequency < T {} ||
              frequency > parameters.fs / T {2}) { return nan(); }
          return response().magnitude_hz(frequency);
     }

     auto response() const -> Response {
          Response snapshot;
          snapshot.fs = parameters.fs;
          if (positive(parameters.fs)) {
               update<true>(parameters.w, W(parameters.zeta), parameters.sigma, W(parameters.gain),
                            nullptr, &snapshot.coefficients);
          }
          return snapshot;
     }

 private:
     struct Parameters {
          T fs {};
          T fc {1000};
          W pi_over_fs {};
          W w {};
          T zeta {1 / std::numbers::sqrt2_v<T>};
          W sigma {W {2} / std::numbers::pi_v<W>};
          T gain {1};
          bool has_cutoff {false};
     };

     static auto positive(const T x) -> bool { return std::isfinite(x) && x > T {}; }
     static auto nan() -> T { return std::numeric_limits<T>::quiet_NaN(); }
     static auto stable_poles(const T q, const T mu) -> bool {
          if constexpr (std::numeric_limits<T>::digits <= std::numeric_limits<double>::digits) {
               const double qd {q}, md {mu};
               const double product {qd * (1.0 + md * md)};
               // Three positive operations have relative error <= gamma_3.
               // This wider margin keeps boundary decisions on the original
               // long-double path; overflow also falls back rather than
               // rejecting a large mu compensated by a very small q.
               constexpr double margin {16 * std::numeric_limits<double>::epsilon()};
               if (std::isfinite(product)) {
                    if (product < 2.0 - margin) { return true; }
                    if (product > 2.0 + margin) { return false; }
               }
          }
          const long double ql {q}, ml {mu};
          return ql * (1.L + ml * ml) < 2.L;
     }
     static auto norm2(const W x, const W y) -> W {
          // Keep ordinary audio parameters on the inlined sqrt path. hypot's
          // scaling is only needed when squaring overflows or underflows.
          const W square {x * x + y * y};
          if (std::isfinite(square) && square > std::numeric_limits<W>::min()) {
               return std::sqrt(square);
          }
          return std::hypot(x, y);
     }
     static auto gain_root(const T x) -> T {
          if constexpr (Order == 1) { return x; }
          if constexpr (Order == 2) { return std::sqrt(x); }
          if constexpr (Order == 4) { return std::sqrt(std::sqrt(x)); }
          return std::pow(x, T {1} / T(Order));
     }

     // Keep the candidate parameter object inside the caller's optimization
     // scope; numerical kernels below take scalar values and may be outlined.
#if defined(__GNUC__) || defined(__clang__)
     [[gnu::always_inline]]
#elif defined(_MSC_VER)
     __forceinline
#endif
     static auto prepare_cutoff(Parameters& p) -> bool {
          if (!positive(p.fc)) { return false; }
          // Hoist the sample-rate division. Extreme rates use the original
          // expression if the cached factor is nonfinite or subnormal.
          const W angle {std::isnormal(p.pi_over_fs)
                              ? W(p.fc) * p.pi_over_fs
                              : std::numbers::pi_v<W> * (W(p.fc) / W(p.fs))};
          if (!(angle > W {}) || !std::isfinite(angle)) { return false; }
          p.w = W {1} / angle;
          if constexpr (Correction != Correction::None) {
               if (!(p.fc < p.fs / T {2})) { return false; }
               if constexpr (Correction == Correction::Omega) {
                    p.w = norm2(W(p.sigma), W {1} / std::tan(angle));
               } else {
                    W sigma_sq;
                    if (angle < W {0.05}) {
                         // 1/a^2 - cot(a)^2, avoiding subtraction of two
                         // enormous almost-equal values. Error O(a^10).
                         const W a2 {angle * angle};
                         sigma_sq = W {2} / W {3} - a2 *
                              (W {1} / W {15} + a2 * (W {2} / W {189} + a2 *
                              (W {1} / W {675} + a2 * (W {2} / W {10395}))));
                    } else {
                         const W ctg {W {1} / std::tan(angle)};
                         sigma_sq = (p.w - ctg) * (p.w + ctg);
                    }
                    if (!(sigma_sq >= W {}) || !std::isfinite(sigma_sq)) { return false; }
                    p.sigma = std::sqrt(sigma_sq);
               }
          }
          return std::isfinite(p.w);
     }

     template<auto N>
     auto process(const State x) -> State {
          if constexpr (N == 0 && Order % 2 != 0) {
               return process<N + 1>(processed6(x));
          } else if constexpr (N < count) {
               return process<N + 1>(processed12<N>(x));
          } else {
               return x;
          }
     }

     auto processed6(const State x) -> State {
          const auto difference {x - z[0][0]};
          const auto theta {difference * b[0][0]};
          const auto y {Type == HighPass ? difference * b[0][1]
                                        : difference * b[0][1] + z[0][0]};
          z[0][0] = z[0][0] + theta;
          return Type == LowShelf ? y * parameters.gain : y;
     }

     template<auto N>
     auto processed12(const State x) -> State {
          const auto difference {x - z[N][0]};
          const auto theta {(difference - z[N][1] * b[N][1]) * b[N][0]};
          // Precombine the output coefficients instead of cancelling terms
          // proportional to mu at run time (especially important for low HP).
          const auto y {(Type == HighPass || Type == BandPass)
                             ? difference * b[N][3] + z[N][1] * b[N][2]
                             : difference * b[N][3] + z[N][1] * b[N][2] + z[N][0]};
          z[N][0] = z[N][0] + theta;
          z[N][1] = -z[N][1] - theta * b[N][1];
          return Type == LowShelf ? y * (parameters.gain * parameters.gain) : y;
     }

     template<auto N>
     static auto response_at(const W c, const W s, const Responses& response) -> W {
          if constexpr (N >= count) {
               return W {1};
          } else {
               const auto& r {response[N]};
               W nr, ni, dr, di;
               if constexpr (N == 0 && Order % 2 != 0) {
                    nr = W(r[0]) * c; ni = W(r[1]) * s;
                    dr = W(r[3]) * c; di = W(r[4]) * s;
               } else {
                    nr = W(r[0]) * c * c - W(r[2]) * s * s;
                    ni = W(r[1]) * s * c;
                    dr = W(r[3]) * c * c - W(r[5]) * s * s;
                    di = W(r[4]) * s * c;
               }
               return std::hypot(nr, ni) / std::hypot(dr, di) * response_at<N + 1>(c, s, response);
          }
     }

     // Return the quadratic coefficient and damping coefficient of
     // 1 + d*s + v*s^2. All terms under square roots are nonnegative.
     static auto denominator(const W w, const W damping, const W sigma) -> std::pair<W, W> {
          const W t {w * w};
          if (sigma == W {}) { return {t, W {2} * w * damping}; }
          const W sigma_sq {sigma * sigma};
          const W a {t - sigma_sq};
          const W v {norm2(a, W {2} * w * damping * sigma)};
          W d;
          if (a >= W {}) {
               const W sum {v + a};
               const W numerator {W {2} * sigma_sq};
               // Preserve the ordinary expression. If only its sum or
               // numerator overflows, halving the denominator keeps the
               // equivalent ratio finite instead of silently replacing it
               // with zero (or rejecting a representable result).
               const W ratio {(std::isfinite(sum) & std::isfinite(numerator))
                                   ? numerator / sum
                                   : sigma_sq / (W {0.5} * v + W {0.5} * a)};
               d = W {2} * w * damping * std::sqrt(W {1} + ratio);
          } else {
               d = std::sqrt(W {2} * (v - a + W {2} * t * damping * damping));
          }
          return {v, d};
     }

     template<auto N>
     static auto section(const W w, const W damping) -> std::pair<W, W> {
          if constexpr (Order == 2 || (is_band<Type> && 2 * N + 1 == Order / 2)) {
               return {w, damping};
          } else if constexpr (is_band<Type>) {
               const W angle {std::numbers::pi_v<W> * W(2 * N + 1) / W(Order)};
               const W sine {damping * std::sin(angle)};
               const W cosine {damping * std::cos(angle)};
               const W a {W {1} - damping * damping};
               const W h {norm2(a, W {2} * cosine)};
               const W m {a >= W {} ? std::numbers::sqrt2_v<W> * std::abs(cosine) / std::sqrt(h + a)
                                    : std::sqrt((h - a) / W {2})};
               const W u {norm2(W {1}, m)};
               const W radius {u + m};
               return {N < Order / 4 ? w / radius : w * radius, sine / u};
          } else {
               const W section_damping {std::cos(std::numbers::pi_v<W> *
                    W(2 * N + 1 - Order % 2) / W(2 * Order))};
               return {w, section_damping * (N + 1 == count ? damping * std::numbers::sqrt2_v<W> : W {1})};
          }
     }

     // Passing only the numerical inputs by value prevents an outlined kernel
     // from forcing the complete parameter transaction into stack storage.
     template<bool ForResponse>
     static auto update6(const W w, const W sigma, const W gain,
                         Coefficients* next, Responses* response) -> bool {
          const W root_gain {std::sqrt(gain)};
          const W wp {Type == HighShelf ? w / root_gain : Type == LowShelf ? w * root_gain : w};
          const W vp {sigma == W {} ? wp : norm2(wp, sigma)};
          const W scale {W {1} / (W {1} + vp)};
          const W a0 {Type == HighPass ? W {} : W {1}};
          const W a1 {Type == HighShelf ? norm2(w * root_gain, sigma)
                     : Type == LowShelf ? norm2(w / root_gain, sigma)
                     : Type == AllPass ? -vp
                     : Type == LowPass ? sigma
                     : w};
          const W output_gain {Type == LowShelf ? gain : W {1}};
          if constexpr (ForResponse) {
               (*response)[0] = {output_gain * a0 * scale, output_gain * a1 * scale, W {}, scale, vp * scale, W {}};
          } else {
               (*next)[0] = {static_cast<T>(W {2} * scale), static_cast<T>((a0 + a1) * scale), T {}, T {}};
               if constexpr (Type == LowShelf) { return std::isfinite(output_gain * a1); }
          }
          return true;
     }

     template<bool ForResponse, auto N>
     static auto update12(const W w0, const W zeta, const W sigma, const W gain,
                          Coefficients* next, Responses* response) -> bool {
          const auto [w, damping] {section<N>(w0, Type == BandShelf ? zeta / gain : zeta)};
          const W root_gain {std::sqrt(gain)};
          const W wp {Type == LowShelf ? w * root_gain : Type == HighShelf ? w / root_gain : w};
          const auto [v, d] {denominator(wp, damping, sigma)};
          const W scale {W {1} / (W {1} + d + v)};
          W a0 {Type == HighPass || Type == BandPass ? W {} : W {1}};
          W a1 {}, a2 {};
          if constexpr (Type == HighPass) {
               a2 = w * w;
          } else if constexpr (Type == BandPass) {
               a1 = W {2} * w * zeta;
               a2 = a1 * sigma;
          } else if constexpr (Type == LowPass) {
               a1 = W {2} * sigma;
               a2 = sigma * sigma;
          } else if constexpr (Type == BandStop) {
               a2 = w0 * w0 - sigma * sigma;
          } else if constexpr (Type == AllPass) {
               a1 = -d;
               a2 = v;
          } else {
               const auto [wa, da] {Type == BandShelf ? section<N>(w0, zeta * gain) : std::pair {w, damping}};
               const W wn {Type == LowShelf ? w / root_gain : Type == HighShelf ? w * root_gain : wa};
               const auto [va, damping_a] {denominator(wn, da, sigma)};
               a1 = damping_a;
               a2 = va;
          }
          const W output_gain {Type == LowShelf ? gain * gain : W {1}};
          if constexpr (ForResponse) {
               (*response)[N] = {output_gain * a0 * scale, output_gain * a1 * scale, output_gain * a2 * scale,
                                scale, d * scale, v * scale};
          } else {
               const W mu {sigma == W {} && std::isnormal(v) ? wp : std::sqrt(v)};
               const W out0 {(a0 + a1 + a2) * scale};
               W out1;
               if constexpr (Type == AllPass) {
                    out1 = W {2} * mu * d * scale;
               } else if constexpr (Type == HighPass || Type == LowPass || Type == BandPass) {
                    if (sigma == W {}) {
                         if constexpr (Type == HighPass) { out1 = mu * scale * (W {1} + d); }
                         else { out1 = -mu * scale * (a0 + a1); }
                    } else {
                         out1 = mu * scale * ((a2 / v) * (W {1} + d) - (a0 + a1));
                    }
               } else {
                    out1 = mu * scale * ((a2 / v) * (W {1} + d) - (a0 + a1));
               }
               (*next)[N] = {static_cast<T>(W {2} * scale), static_cast<T>(mu),
                            static_cast<T>(out1), static_cast<T>(out0)};
               // Preserve rejection when positive nominal damping underflows
               // in the normalized polynomial. The other response-domain
               // checks follow from the finite output coefficients and q>0.
               if (!(d * scale > W {})) { return false; }
               if constexpr (Type == LowShelf) {
                    return std::isfinite(output_gain * a1) && std::isfinite(output_gain * a2);
               }
          }
          return true;
     }

     template<bool ForResponse, auto N = 0>
     static auto update(const W w, const W zeta, const W sigma, const W gain,
                        Coefficients* next, Responses* response) -> bool {
          bool valid;
          if constexpr (N == 0 && Order % 2 != 0) { valid = update6<ForResponse>(w, sigma, gain, next, response); }
          else { valid = update12<ForResponse, N>(w, zeta, sigma, gain, next, response); }
          if (!valid) { return false; }
          if constexpr (N + 1 < count) { return update<ForResponse, N + 1>(w, zeta, sigma, gain, next, response); }
          return true;
     }

#if defined(__GNUC__) || defined(__clang__)
     [[gnu::always_inline]]
#elif defined(_MSC_VER)
     __forceinline
#endif
     auto commit(const Parameters& p) -> bool {
          if constexpr (Type == LowShelf && Order > 1) {
               if (!std::isfinite(p.gain * p.gain)) { return false; }
          }
          Coefficients next {};
          if (!update<false>(p.w, W(p.zeta), p.sigma, W(p.gain), &next, nullptr)) { return false; }
          for (auto i = 0; i < count; ++i) {
               if (!(next[i][0] > T {} && next[i][0] < T {2})) { return false; }
               if (i >= Order % 2) {
                    if (!(next[i][1] > T {}) ||
                        !(std::isfinite(next[i][2]) & std::isfinite(next[i][3]))) { return false; }
                    // Positive nominal damping does not guarantee stable poles
                    // after q and mu are independently rounded for storage.
                    // det(D) = -1 + q*(1+mu^2); the other two Schur inequalities
                    // follow from q>0, mu>0. Reject instead of silently changing Q.
                    if (!stable_poles(next[i][0], next[i][1])) { return false; }
               } else {
                    if (!std::isfinite(next[i][1])) { return false; }
               }
          }
          // Assign fields explicitly: GCC otherwise materializes and reloads
          // the whole candidate across partially overlapping scalar stores.
          // Unchanged fields disappear when the setter is inlined.
          parameters.fs = p.fs;
          parameters.fc = p.fc;
          parameters.pi_over_fs = p.pi_over_fs;
          parameters.w = p.w;
          parameters.zeta = p.zeta;
          parameters.sigma = p.sigma;
          parameters.gain = p.gain;
          parameters.has_cutoff = p.has_cutoff;
          b = next;
          return true;
     }

     Parameters parameters {};
     Coefficients b {};
     std::array<std::array<State, 2>, count> z {};
};
} // namespace Linear
} // namespace ivantsov
