#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>

#include <cstdint>
#include <type_traits>

namespace avnd
{

/**
 * Device memory: a raw pointer to GPU-resident data.
 *
 * This is the common denominator across CUDA (cudaMalloc),
 * HIP (hipMalloc), and SYCL USM (sycl::malloc_device).
 * The pointer type is void* — the processor casts it to the
 * appropriate type and uses it with the GPU API of its choice.
 */
template <typename T>
concept device_memory = requires(T t) {
  { t.device_ptr } -> std::convertible_to<void*>;
  { t.byte_size } -> std::convertible_to<uint64_t>;
};

/**
 * A port carrying device memory.
 * The port has a .mem member satisfying the device_memory concept.
 */
template <typename T>
concept device_memory_port
    = device_memory<std::decay_t<decltype(std::declval<T>().mem)>>;

/**
 * A processor that requests a GPU compute context.
 * Detected by the presence of a .gpu member with at least device_index.
 * The binding fills this before calling operator().
 */
template <typename T>
concept gpu_compute_context = requires(T t) {
  { t.gpu.device_index } -> std::convertible_to<int>;
};

}
