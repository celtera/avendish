#pragma once
#include <functional>
#include <vector>

namespace halp
{
template <typename Port>
struct dynamic_port
{
  std::vector<Port> ports{};
  std::function<void(int)> request_port_resize;
};
}
