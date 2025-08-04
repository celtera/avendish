#pragma once
#include <cassert>
#include <utility>

namespace avnd
{
template <typename T, auto Header, typename... Args>
T* init_cpp_object(void* obj, Args&&... args)
{
  if(obj)
  {
    using header_type = std::remove_cvref_t<decltype(std::declval<T>().*Header)>;
    header_type tmp;
    memcpy(&tmp, obj, sizeof(header_type));
    auto x = new(obj) T{};
    auto& header = x->*Header;
    assert((void*)&header == (void*)x);
    assert((void*)&header == (void*)obj);
    memcpy(&header, &tmp, sizeof(header_type));
    x->init(std::forward<Args>(args)...);
    return x;
  }
  return nullptr;
}
}
