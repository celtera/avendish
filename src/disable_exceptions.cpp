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
