#pragma once
#include <halp/modules.hpp>

#include <memory>
#include <mutex>

HALP_MODULE_EXPORT
namespace halp
{
/**
 * Automatically manages a shared instance for an object, which will 
 * be created automatically & deleted when not used anymore
 */
template <typename T>
std::shared_ptr<T> shared_instance()
{
  static std::mutex mut;
  static std::weak_ptr<T> cache;

  std::lock_guard _{mut};

  if(auto ptr = cache.lock())
  {
    return ptr;
  }
  else
  {
    auto shared = std::make_shared<T>();
    cache = shared;
    return shared;
  }
}
}
