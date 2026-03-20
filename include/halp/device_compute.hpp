#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <cstdint>
#include <string_view>

namespace halp
{

/**
 * Device memory buffer: a raw pointer to GPU-resident data.
 *
 * Works with any GPU compute backend:
 *   - CUDA:  device_ptr from cudaMalloc, stream is cudaStream_t
 *   - HIP:   device_ptr from hipMalloc,  stream is hipStream_t
 *   - SYCL:  device_ptr from sycl::malloc_device, stream is sycl::queue*
 *
 * The processor casts device_ptr and stream to the native types
 * of its chosen GPU API.
 */
struct device_mem_buffer
{
  void* device_ptr{};
  uint64_t byte_size{};

  /// Opaque stream/queue handle.
  /// Cast to cudaStream_t, hipStream_t, sycl::queue*, etc.
  void* stream{};

  /// Which GPU device this memory lives on.
  int device_index{-1};

  bool changed{};
};

/**
 * GPU compute context.
 *
 * A processor declares a member of this type (conventionally named `gpu`)
 * to request GPU compute resources from the host.
 * The binding fills it before each call to operator().
 */
struct gpu_worker
{
  int device_index{-1};

  /// Opaque stream/queue handle for ordering GPU operations.
  void* stream{};

  /// Opaque native context handle (CUcontext, hipCtx_t, sycl::context*, etc.)
  void* native_context{};
};

template <static_string lit>
struct device_memory_input
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  device_mem_buffer mem{};
};

template <static_string lit>
struct device_memory_output
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  device_mem_buffer mem{};
};

}
