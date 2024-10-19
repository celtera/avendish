#pragma once
#include <score/plugins/UuidKey.hpp>

namespace oscr
{
inline QString fromStringView(std::string_view v)
{
  return QString::fromUtf8(v.data(), v.size());
}

template <typename N>
consteval score::uuid_t uuid_from_string()
{
  if constexpr(requires {
                 { N::uuid() } -> std::convertible_to<score::uuid_t>;
               })
  {
    return N::uuid();
  }
  else
  {
    static constexpr const char* str = N::uuid();
    return score::uuids::string_generator::compute(str, str + 37);
  }
}

template <typename Node>
score::uuids::uuid make_field_uuid(uint64_t is_input, uint64_t index)
{
  score::uuid_t node_uuid = uuid_from_string<Node>();
  uint64_t dat[2];

  memcpy(dat, node_uuid.data, 16);

  // FIXME does not look like it's enough
  dat[0] ^= is_input;
  dat[1] ^= index;

  memcpy(node_uuid.data, dat, 16);

  return node_uuid;
}
}
