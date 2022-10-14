#include <exception>
#include <cstdlib>

namespace boost {
void throw_exception(std::exception const &e) { std::abort(); }
} // namespace boost
