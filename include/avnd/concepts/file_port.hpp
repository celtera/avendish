#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/enums.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{

template <typename T>
concept file = requires(T t)
{
  t.filename;
};

template <typename T>
concept file_port = requires(T t)
{
  {
    t.file
  } -> file;
};


template <typename T>
concept raw_file = file<T> && requires(T t)
{
  t.bytes;
};

template <typename T>
concept raw_file_port = requires(T t)
{
  {
    t.file
    } -> raw_file;
};

AVND_DEFINE_TAG(file_watch)
}
