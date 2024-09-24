#pragma once
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/struct_reflection.hpp>

namespace avnd
{

template <typename T>
concept type_wrapper = requires(T t) { sizeof(typename T::wrapped_type); };

}
