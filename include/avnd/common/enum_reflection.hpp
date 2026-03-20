#pragma once

#if __has_include(<magic_enum.hpp>)
#include <magic_enum.hpp>
#elif __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#error magic_enum is required
#endif
