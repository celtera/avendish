#pragma once
#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>

// ! this file ported from libc++ strstream, now sometimes removed
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

namespace compat
{
class strstreambuf : public std::streambuf
{
public:
  strstreambuf()
      : strstreambuf(0)
  {
  }
  explicit strstreambuf(std::streamsize __alsize);
  strstreambuf(void* (*__palloc)(size_t), void (*__pfree)(void*));
  strstreambuf(char* __gnext, std::streamsize __n, char* __pbeg = nullptr);
  strstreambuf(const char* __gnext, std::streamsize __n);

  strstreambuf(signed char* __gnext, std::streamsize __n, signed char* __pbeg = nullptr);
  strstreambuf(const signed char* __gnext, std::streamsize __n);
  strstreambuf(
      unsigned char* __gnext, std::streamsize __n, unsigned char* __pbeg = nullptr);
  strstreambuf(const unsigned char* __gnext, std::streamsize __n);

  inline strstreambuf(strstreambuf&& __rhs);
  inline strstreambuf& operator=(strstreambuf&& __rhs);

  ~strstreambuf() override;

  void swap(strstreambuf& __rhs);

  void freeze(bool __freezefl = true);
  char* str();
  int pcount() const;

protected:
  int_type overflow(int_type __c = EOF) override;
  int_type pbackfail(int_type __c = EOF) override;
  int_type underflow() override;
  pos_type seekoff(
      off_type __off, std::ios_base::seekdir __way,
      std::ios_base::openmode __which = std::ios_base::in | std::ios_base::out) override;
  pos_type seekpos(
      pos_type __sp,
      std::ios_base::openmode __which = std::ios_base::in | std::ios_base::out) override;

private:
  typedef unsigned __mode_type;
  static const __mode_type __allocated = 0x01;
  static const __mode_type __constant = 0x02;
  static const __mode_type __dynamic = 0x04;
  static const __mode_type __frozen = 0x08;
  static const std::streamsize __default_alsize = 4096;

  __mode_type __strmode_;
  std::streamsize __alsize_;
  void* (*__palloc_)(size_t);
  void (*__pfree_)(void*);

  void __init(char* __gnext, std::streamsize __n, char* __pbeg);
};

inline strstreambuf::strstreambuf(strstreambuf&& __rhs)
    : std::streambuf(__rhs)
    , __strmode_(__rhs.__strmode_)
    , __alsize_(__rhs.__alsize_)
    , __palloc_(__rhs.__palloc_)
    , __pfree_(__rhs.__pfree_)
{
  __rhs.setg(nullptr, nullptr, nullptr);
  __rhs.setp(nullptr, nullptr);
}

inline strstreambuf& strstreambuf::operator=(strstreambuf&& __rhs)
{
  if(eback() && (__strmode_ & __allocated) != 0 && (__strmode_ & __frozen) == 0)
  {
    if(__pfree_)
      __pfree_(eback());
    else
      delete[] eback();
  }
  std::streambuf::operator=(__rhs);
  __strmode_ = __rhs.__strmode_;
  __alsize_ = __rhs.__alsize_;
  __palloc_ = __rhs.__palloc_;
  __pfree_ = __rhs.__pfree_;
  __rhs.setg(nullptr, nullptr, nullptr);
  __rhs.setp(nullptr, nullptr);
  return *this;
}

class istrstream : public std::istream
{
public:
  inline explicit istrstream(const char* __s)
      : std::istream(&__sb_)
      , __sb_(__s, 0)
  {
  }
  inline explicit istrstream(char* __s)
      : std::istream(&__sb_)
      , __sb_(__s, 0)
  {
  }
  inline istrstream(const char* __s, std::streamsize __n)
      : std::istream(&__sb_)
      , __sb_(__s, __n)
  {
  }
  inline istrstream(char* __s, std::streamsize __n)
      : std::istream(&__sb_)
      , __sb_(__s, __n)
  {
  }

  inline istrstream(istrstream&& __rhs) // extension
      : std::istream(std::move(static_cast<std::istream&>(__rhs)))
      , __sb_(std::move(__rhs.__sb_))
  {
    std::istream::set_rdbuf(&__sb_);
  }

  inline istrstream& operator=(istrstream&& __rhs)
  {
    __sb_ = std::move(__rhs.__sb_);
    std::istream::operator=(std::move(__rhs));
    return *this;
  }

  ~istrstream() override = default;

  inline void swap(istrstream& __rhs)
  {
    std::istream::swap(__rhs);
    __sb_.swap(__rhs.__sb_);
  }

  inline strstreambuf* rdbuf() const { return const_cast<strstreambuf*>(&__sb_); }
  inline char* str() { return __sb_.str(); }

private:
  strstreambuf __sb_;
};

class ostrstream : public std::ostream
{
public:
  inline ostrstream()
      : std::ostream(&__sb_)
  {
  }
  inline ostrstream(
      char* __s, int __n, std::ios_base::openmode __mode = std::ios_base::out)
      : std::ostream(&__sb_)
      , __sb_(__s, __n, __s + (__mode & std::ios::app ? std::strlen(__s) : 0))
  {
  }

  inline ostrstream(ostrstream&& __rhs) // extension
      : std::ostream(std::move(static_cast<std::ostream&>(__rhs)))
      , __sb_(std::move(__rhs.__sb_))
  {
    std::ostream::set_rdbuf(&__sb_);
  }

  inline ostrstream& operator=(ostrstream&& __rhs)
  {
    __sb_ = std::move(__rhs.__sb_);
    std::ostream::operator=(std::move(__rhs));
    return *this;
  }

  ~ostrstream() override = default;

  inline void swap(ostrstream& __rhs)
  {
    std::ostream::swap(__rhs);
    __sb_.swap(__rhs.__sb_);
  }

  inline strstreambuf* rdbuf() const { return const_cast<strstreambuf*>(&__sb_); }
  inline void freeze(bool __freezefl = true) { __sb_.freeze(__freezefl); }
  inline char* str() { return __sb_.str(); }
  inline int pcount() const { return __sb_.pcount(); }

private:
  strstreambuf __sb_; // exposition only
};

class strstream : public std::iostream
{
public:
  // Types
  typedef char char_type;
  typedef std::char_traits<char>::int_type int_type;
  typedef std::char_traits<char>::pos_type pos_type;
  typedef std::char_traits<char>::off_type off_type;

  // constructors/destructor
  inline strstream()
      : std::iostream(&__sb_)
  {
  }
  inline strstream(
      char* __s, int __n,
      std::ios_base::openmode __mode = std::ios_base::in | std::ios_base::out)
      : std::iostream(&__sb_)
      , __sb_(__s, __n, __s + (__mode & std::ios::app ? std::strlen(__s) : 0))
  {
  }

  inline strstream(strstream&& __rhs) // extension
      : std::iostream(std::move(static_cast<std::iostream&>(__rhs)))
      , __sb_(std::move(__rhs.__sb_))
  {
    std::iostream::set_rdbuf(&__sb_);
  }

  inline strstream& operator=(strstream&& __rhs)
  {
    __sb_ = std::move(__rhs.__sb_);
    std::iostream::operator=(std::move(__rhs));
    return *this;
  }

  ~strstream() override = default;

  inline void swap(strstream& __rhs)
  {
    std::iostream::swap(__rhs);
    __sb_.swap(__rhs.__sb_);
  }

  // Members:
  inline strstreambuf* rdbuf() const { return const_cast<strstreambuf*>(&__sb_); }
  inline void freeze(bool __freezefl = true) { __sb_.freeze(__freezefl); }
  inline int pcount() const { return __sb_.pcount(); }
  inline char* str() { return __sb_.str(); }

private:
  strstreambuf __sb_; // exposition only
};

inline strstreambuf::strstreambuf(std::streamsize __alsize)
    : __strmode_(__dynamic)
    , __alsize_(__alsize)
    , __palloc_(nullptr)
    , __pfree_(nullptr)
{
}

inline strstreambuf::strstreambuf(void* (*__palloc)(size_t), void (*__pfree)(void*))
    : __strmode_(__dynamic)
    , __alsize_(__default_alsize)
    , __palloc_(__palloc)
    , __pfree_(__pfree)
{
}

inline void strstreambuf::__init(char* __gnext, std::streamsize __n, char* __pbeg)
{
  if(__n == 0)
    __n = static_cast<std::streamsize>(strlen(__gnext));
  else if(__n < 0)
    __n = INT_MAX;
  if(__pbeg == nullptr)
    setg(__gnext, __gnext, __gnext + __n);
  else
  {
    setg(__gnext, __gnext, __pbeg);
    setp(__pbeg, __pbeg + __n);
  }
}

inline strstreambuf::strstreambuf(char* __gnext, std::streamsize __n, char* __pbeg)
    : __strmode_()
    , __alsize_(__default_alsize)
    , __palloc_(nullptr)
    , __pfree_(nullptr)
{
  __init(__gnext, __n, __pbeg);
}

inline strstreambuf::strstreambuf(const char* __gnext, std::streamsize __n)
    : __strmode_(__constant)
    , __alsize_(__default_alsize)
    , __palloc_(nullptr)
    , __pfree_(nullptr)
{
  __init(const_cast<char*>(__gnext), __n, nullptr);
}

inline strstreambuf::strstreambuf(
    signed char* __gnext, std::streamsize __n, signed char* __pbeg)
    : __strmode_()
    , __alsize_(__default_alsize)
    , __palloc_(nullptr)
    , __pfree_(nullptr)
{
  __init(
      const_cast<char*>(reinterpret_cast<const char*>(__gnext)), __n,
      reinterpret_cast<char*>(__pbeg));
}

inline strstreambuf::strstreambuf(const signed char* __gnext, std::streamsize __n)
    : __strmode_(__constant)
    , __alsize_(__default_alsize)
    , __palloc_(nullptr)
    , __pfree_(nullptr)
{
  __init(const_cast<char*>(reinterpret_cast<const char*>(__gnext)), __n, nullptr);
}

inline strstreambuf::strstreambuf(
    unsigned char* __gnext, std::streamsize __n, unsigned char* __pbeg)
    : __strmode_()
    , __alsize_(__default_alsize)
    , __palloc_(nullptr)
    , __pfree_(nullptr)
{
  __init(
      const_cast<char*>(reinterpret_cast<const char*>(__gnext)), __n,
      reinterpret_cast<char*>(__pbeg));
}

inline strstreambuf::strstreambuf(const unsigned char* __gnext, std::streamsize __n)
    : __strmode_(__constant)
    , __alsize_(__default_alsize)
    , __palloc_(nullptr)
    , __pfree_(nullptr)
{
  __init(const_cast<char*>(reinterpret_cast<const char*>(__gnext)), __n, nullptr);
}

inline strstreambuf::~strstreambuf()
{
  if(eback() && (__strmode_ & __allocated) != 0 && (__strmode_ & __frozen) == 0)
  {
    if(__pfree_)
      __pfree_(eback());
    else
      delete[] eback();
  }
}

inline void strstreambuf::swap(strstreambuf& __rhs)
{
  std::streambuf::swap(__rhs);
  std::swap(__strmode_, __rhs.__strmode_);
  std::swap(__alsize_, __rhs.__alsize_);
  std::swap(__palloc_, __rhs.__palloc_);
  std::swap(__pfree_, __rhs.__pfree_);
}

inline void strstreambuf::freeze(bool __freezefl)
{
  if(__strmode_ & __dynamic)
  {
    if(__freezefl)
      __strmode_ |= __frozen;
    else
      __strmode_ &= ~__frozen;
  }
}

inline char* strstreambuf::str()
{
  if(__strmode_ & __dynamic)
    __strmode_ |= __frozen;
  return eback();
}

inline int strstreambuf::pcount() const
{
  return static_cast<int>(pptr() - pbase());
}

inline strstreambuf::int_type strstreambuf::overflow(int_type __c)
{
  if(__c == EOF)
    return int_type(0);
  if(pptr() == epptr())
  {
    if((__strmode_ & __dynamic) == 0 || (__strmode_ & __frozen) != 0)
      return int_type(EOF);
    size_t old_size = static_cast<size_t>((epptr() ? epptr() : egptr()) - eback());
    size_t new_size = std::max<size_t>(static_cast<size_t>(__alsize_), 2 * old_size);
    if(new_size == 0)
      new_size = __default_alsize;
    char* buf = nullptr;
    if(__palloc_)
      buf = static_cast<char*>(__palloc_(new_size));
    else
      buf = new char[new_size];
    if(buf == nullptr)
      return int_type(EOF);
    if(old_size != 0)
    {
      assert(eback());
      memcpy(buf, eback(), static_cast<size_t>(old_size));
    }
    ptrdiff_t ninp = gptr() - eback();
    ptrdiff_t einp = egptr() - eback();
    ptrdiff_t nout = pptr() - pbase();
    if(__strmode_ & __allocated)
    {
      if(__pfree_)
        __pfree_(eback());
      else
        delete[] eback();
    }
    setg(buf, buf + ninp, buf + einp);
    setp(buf + einp, buf + new_size);
    pbump(nout);
    __strmode_ |= __allocated;
  }
  *pptr() = static_cast<char>(__c);
  pbump(1);
  return int_type(static_cast<unsigned char>(__c));
}

inline strstreambuf::int_type strstreambuf::pbackfail(int_type __c)
{
  if(eback() == gptr())
    return EOF;
  if(__c == EOF)
  {
    gbump(-1);
    return int_type(0);
  }
  if(__strmode_ & __constant)
  {
    if(gptr()[-1] == static_cast<char>(__c))
    {
      gbump(-1);
      return __c;
    }
    return EOF;
  }
  gbump(-1);
  *gptr() = static_cast<char>(__c);
  return __c;
}

inline strstreambuf::int_type strstreambuf::underflow()
{
  if(gptr() == egptr())
  {
    if(egptr() >= pptr())
      return EOF;
    setg(eback(), gptr(), pptr());
  }
  return int_type(static_cast<unsigned char>(*gptr()));
}

inline strstreambuf::pos_type strstreambuf::seekoff(
    off_type __off, std::ios_base::seekdir __way, std::ios_base::openmode __which)
{
  bool pos_in = (__which & std::ios::in) != 0;
  bool pos_out = (__which & std::ios::out) != 0;
  switch(__way)
  {
    case std::ios::beg:
    case std::ios::end:
      if(!pos_in && !pos_out)
        return pos_type(off_type(-1));
      break;
    case std::ios::cur:
      if(pos_in == pos_out)
        return pos_type(off_type(-1));
      break;
    default:
      break;
  }

  if(pos_in && gptr() == nullptr)
    return pos_type(off_type(-1));
  if(pos_out && pptr() == nullptr)
    return pos_type(off_type(-1));

  off_type newoff;
  char* seekhigh = epptr() ? epptr() : egptr();
  switch(__way)
  {
    case std::ios::beg:
      newoff = 0;
      break;
    case std::ios::cur:
      newoff = (pos_in ? gptr() : pptr()) - eback();
      break;
    case std::ios::end:
      newoff = seekhigh - eback();
      break;
    default:
      std::terminate();
  }
  newoff += __off;
  if(newoff < 0 || newoff > seekhigh - eback())
    return pos_type(off_type(-1));

  char* newpos = eback() + newoff;
  if(pos_in)
    setg(eback(), newpos, std::max(newpos, egptr()));
  if(pos_out)
  {
    // min(pbase, newpos), newpos, epptr()
    __off = epptr() - newpos;
    setp(std::min(pbase(), newpos), epptr());
    pbump((epptr() - pbase()) - __off);
  }
  return pos_type(newoff);
}

inline strstreambuf::pos_type
strstreambuf::seekpos(pos_type __sp, std::ios_base::openmode __which)
{
  bool pos_in = (__which & std::ios::in) != 0;
  bool pos_out = (__which & std::ios::out) != 0;
  if(!pos_in && !pos_out)
    return pos_type(off_type(-1));

  if((pos_in && gptr() == nullptr) || (pos_out && pptr() == nullptr))
    return pos_type(off_type(-1));

  off_type newoff = __sp;
  char* seekhigh = epptr() ? epptr() : egptr();
  if(newoff < 0 || newoff > seekhigh - eback())
    return pos_type(off_type(-1));

  char* newpos = eback() + newoff;
  if(pos_in)
    setg(eback(), newpos, std::max(newpos, egptr()));
  if(pos_out)
  {
    // min(pbase, newpos), newpos, epptr()
    off_type temp = epptr() - newpos;
    setp(std::min(pbase(), newpos), epptr());
    pbump((epptr() - pbase()) - temp);
  }
  return pos_type(newoff);
}
}
