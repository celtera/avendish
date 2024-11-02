#pragma once
#include <halp/modules.hpp>

#include <functional>
HALP_MODULE_EXPORT
namespace halp
{

template <typename T>
struct transaction
{
  std::function<void()> start;
  std::function<void(const T&)> update;
  std::function<void()> commit;
  std::function<void()> rollback;
};

}
