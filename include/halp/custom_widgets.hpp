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

// The transaction of a widget that edits several parameters at once (see
// halp::custom_multi_control): a gesture can change any of them, and the
// host records it as one edit. Values are normalized to [0, 1] over each
// parameter's range.
struct multi_transaction
{
  std::function<void()> start;
  std::function<void(int index, double normalized)> update;
  std::function<void()> commit;
  std::function<void()> rollback;
};

}
