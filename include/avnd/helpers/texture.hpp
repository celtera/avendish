#pragma once
#include <boost/container/vector.hpp>
namespace avnd
{
struct rgba_texture {
  enum format { RGBA };
  unsigned char* bytes{};
  int width{};
  int height{};
  bool changed{};

  /* FIXME the allocation should not be managed by the plug-in */
  static auto allocate(int width, int height)
  {
    using namespace boost::container;
    return vector<unsigned char>(width * height * 4, default_init);
  }

  void update(unsigned char* data, int w, int h)
  { bytes = data; width = w; height = h; changed = true; }
};
}
