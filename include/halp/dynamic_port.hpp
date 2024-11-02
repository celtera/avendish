#pragma once
#include <halp/modules.hpp>

#include <functional>
#include <vector>
HALP_MODULE_EXPORT
namespace halp
{
template <typename Port>
struct dynamic_port
{
  std::vector<Port> ports{};
  std::function<void(int)> request_port_resize;
};
}
