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

// For halp::custom_multi_control: one gesture, one edit. Values normalized to [0, 1].
struct multi_transaction
{
  std::function<void()> start;
  std::function<void(int index, double normalized)> update;
  std::function<void()> commit;
  std::function<void()> rollback;
};

}
