// boost::throw_exception is only required when Boost is compiled with
// BOOST_NO_EXCEPTIONS (the DisableExceptions interface). When an add-on opts into
// exceptions via AVND_USES_EXCEPTIONS, DisableExceptions carries no such define, so
// this stub must be dropped -- otherwise it would override Boost's real, throwing
// implementation with one that abort()s. AVND_DISABLE_EXCEPTIONS is set (PRIVATE)
// by avendish.disableexceptions.cmake only in the disabling case.
#ifdef AVND_DISABLE_EXCEPTIONS
#include <exception>
#include <cstdlib>

namespace boost {
struct source_location;
void throw_exception(std::exception const &e) { std::abort(); }
void throw_exception(std::exception const& e, boost::source_location const&)
{
  std::abort();
}
} // namespace boost
#else
// Keep the translation unit non-empty so the static library always has a symbol
// (avoids "archive has no index"/empty-object warnings on some toolchains).
namespace avnd::detail { extern const int disable_exceptions_noop; const int disable_exceptions_noop = 0; }
#endif
