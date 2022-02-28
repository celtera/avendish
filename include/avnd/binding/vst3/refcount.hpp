#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <atomic>
namespace stv3
{
struct refcount
{
  std::atomic<int> m_refcount{1};
};
}
