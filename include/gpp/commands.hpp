#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <gpp/generators.hpp>

// FIXME: mpark::variant gives the best results
#include <variant>

namespace bv2 = std;
namespace gpp
{
struct buffer_handle_t;
using buffer_handle = buffer_handle_t*;
struct texture_handle_t;
using texture_handle = texture_handle_t*;
struct sampler_handle_t;
using sampler_handle = sampler_handle_t*;

// Define our commands
struct static_allocation
{
  enum
  {
    allocation,
    static_,
    storage
  };
  using return_type = buffer_handle;
  int binding;
  int size;
};

struct static_upload
{
  enum
  {
    upload,
    static_
  };
  using return_type = void;
  buffer_handle handle;
  int offset;
  int size;
  void* data;
};

struct dynamic_vertex_allocation
{
  enum
  {
    allocation,
    dynamic,
    vertex
  };
  using return_type = buffer_handle;
  int binding;
  int size;
};
struct dynamic_vertex_upload
{
  enum
  {
    upload,
    dynamic,
    vertex
  };
  using return_type = void;
  buffer_handle handle;
  int offset;
  int size;
  void* data;
};

struct dynamic_index_allocation
{
  enum
  {
    allocation,
    dynamic,
    index
  };
  using return_type = buffer_handle;
  int binding;
  int size;
};
struct dynamic_index_upload
{
  enum
  {
    upload,
    dynamic,
    index
  };
  using return_type = void;
  buffer_handle handle;
  int offset;
  int size;
  void* data;
};

struct dynamic_ubo_allocation
{
  enum
  {
    allocation,
    dynamic,
    ubo
  };
  using return_type = buffer_handle;
  int binding;
  int size;
};
struct dynamic_ubo_upload
{
  enum
  {
    upload,
    dynamic,
    ubo
  };
  using return_type = void;
  buffer_handle handle;
  int offset;
  int size;
  void* data;
};

struct sampler_allocation
{
  enum
  {
    allocation,
    sampler
  };
  using return_type = sampler_handle;
};
struct texture_allocation
{
  enum
  {
    allocation,
    texture
  };
  using return_type = texture_handle;
  int binding;
  int width;
  int height;
};

struct texture_upload
{
  enum
  {
    upload,
    texture
  };
  using return_type = void;
  texture_handle handle;
  int offset;
  int size;
  void* data;
};

struct get_ubo_handle
{
  enum
  {
    getter,
    ubo
  };
  using return_type = buffer_handle;
  int binding;
};
struct get_texture_handle
{
  enum
  {
    getter,
    texture
  };
  using return_type = texture_handle;
  int binding;
};

struct buffer_release
{
  enum
  {
    deallocation,
    vertex,
    index
  };
  using return_type = void;
  buffer_handle handle;
};

struct ubo_release
{
  enum
  {
    deallocation,
    ubo
  };
  using return_type = void;
  buffer_handle handle;
};

struct sampler_release
{
  enum
  {
    deallocation,
    texture
  };
  using return_type = void;
  texture_handle handle;
};

struct texture_release
{
  enum
  {
    deallocation,
    texture
  };
  using return_type = void;
  texture_handle handle;
};

struct begin_compute_pass
{
  enum
  {
    compute,
    begin
  };
  using return_type = void;
};

struct end_compute_pass
{
  enum
  {
    compute,
    end
  };
  using return_type = void;
};

struct compute_dispatch
{
  enum
  {
    compute,
    dispatch
  };
  using return_type = void;
  int x, y, z;
};

struct buffer_view
{
  const char* data;
  std::size_t size;
};
struct texture_view
{
  const char* data;
  std::size_t size;
};

struct buffer_readback_handle_t;
struct texture_readback_handle_t;
using buffer_readback_handle = buffer_readback_handle_t*;
using texture_readback_handle = texture_readback_handle_t*;

struct buffer_awaiter
{
  enum
  {
    readback,
    await,
    buffer
  };
  using return_type = buffer_view;
  buffer_readback_handle handle;
};
struct texture_awaiter
{
  enum
  {
    readback,
    await,
    texture
  };
  using return_type = texture_view;
  texture_readback_handle handle;
};
struct readback_buffer
{
  enum
  {
    readback,
    request,
    buffer
  };
  using return_type = buffer_awaiter;
  buffer_handle handle;
  int offset;
  int size;
};
struct readback_texture
{
  enum
  {
    readback,
    request,
    texture
  };
  using return_type = texture_awaiter;
  texture_handle handle;
};

// Define what the update() can do
using update_action = bv2::variant<
    static_allocation, static_upload, dynamic_vertex_allocation, dynamic_vertex_upload,
    buffer_release, dynamic_index_allocation, dynamic_index_upload,
    dynamic_ubo_allocation, dynamic_ubo_upload, ubo_release, sampler_allocation,
    sampler_release, texture_allocation, texture_upload, texture_release,
    get_ubo_handle>;
using update_handle
    = bv2::variant<bv2::monostate, buffer_handle, texture_handle, sampler_handle>;
using co_update = gpp::generator<update_action, update_handle>;

// Define what the release() can do
using release_action
    = bv2::variant<buffer_release, ubo_release, sampler_release, texture_release>;
using co_release = gpp::generator<release_action, void>;

// Define what the dispatch(), for compute, can do

using dispatch_action = bv2::variant<
    begin_compute_pass, end_compute_pass, compute_dispatch, readback_buffer,
    readback_texture, buffer_awaiter, texture_awaiter>;
using dispatch_handle = bv2::variant<
    bv2::monostate, buffer_awaiter, texture_awaiter, buffer_view, texture_view>;
using co_dispatch = gpp::generator<dispatch_action, dispatch_handle>;

}
