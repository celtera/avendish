#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#if __has_include(<span>)
#include <span>
namespace avnd
{
template <typename T, size_t E = std::dynamic_extent>
using span = std::span<T, E>;
}
#elif __has_include(<gsl/span>)
#include <gsl/span>
namespace avnd
{
template <typename T, size_t E = gsl::dynamic_extent>
using span = gsl::span<T, E>;
}
#endif
