#pragma once

#include <QString>

#include <string_view>

namespace oscr
{

inline QString fromStringView(std::string_view v)
{
  return QString::fromUtf8(v.data(), v.size());
}

template <typename T>
inline QString getName(const T& t)
{
  return fromStringView(avnd::get_name(t));
}

template <typename T>
inline QString getName()
{
  return fromStringView(avnd::get_name<T>());
}

inline QByteArray utf8FromStringView(std::string_view v)
{
  return QByteArray(v.data(), v.size());
}

template <typename T>
inline QByteArray getUtf8Name(const T& t)
{
  return utf8FromStringView(avnd::get_name(t));
}

template <typename T>
inline QByteArray getUtf8Name()
{
  return utf8FromStringView(avnd::get_name<T>());
}
}
