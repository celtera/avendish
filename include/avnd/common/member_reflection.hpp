#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */


namespace avnd
{

template <auto f>
struct member_reflection;

template <typename T, typename R, R (T::*member)>
struct member_reflection<member>
{
  using member_type = R;
  using class_type = T;
};

template<auto M>
using member_type = typename member_reflection<M>::member_type;
}
