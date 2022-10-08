#pragma once
#include <avnd/binding/ossia/value.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/parameter.hpp>
#include <ossia/network/value/value.hpp>

namespace oscr
{

struct to_ossia_value_impl
{
  ossia::value& val;

  template <typename F>
  void to_vector(const F& f)
  {
    constexpr int fields = avnd::pfr::tuple_size_v<F>;
    std::vector<ossia::value> v;
    v.resize(fields);

    int k = 0;
    avnd::pfr::for_each_field(f, [&](const auto& f) { to_ossia_value_impl{v[k++]}(f); });

    val = std::move(v);
  }

  template <typename F>
  requires std::is_aggregate_v<F>
  void operator()(const F& f)
  {
    constexpr int fields = avnd::pfr::tuple_size_v<F>;
    if constexpr(vecf_compatible<F>())
    {
      if constexpr(fields == 2)
      {
        auto [x, y] = f;
        val = ossia::vec2f{x, y};
      }
      else if constexpr(fields == 3)
      {
        auto [x, y, z] = f;
        val = ossia::vec3f{x, y, z};
      }
      else if constexpr(fields == 4)
      {
        auto [x, y, z, w] = f;
        val = ossia::vec4f{x, y, z, w};
      }
      else
      {
        to_vector(f);
      }
    }
    else
    {
      to_vector(f);
    }
  }

  template <typename F>
  requires(std::is_integral_v<F>) void operator()(const F& f) { val = (int)f; }

  template <typename F>
  requires(std::is_floating_point_v<F>) void operator()(const F& f) { val = (float)f; }

  void operator()(std::string_view f) { val = std::string(f); }

  void operator()(const std::string& f) { val = f; }

  void operator()(bool f) { val = f; }

  template <template <typename> typename T, typename V>
  requires avnd::optional_ish<T<V>>
  void operator()(const T<V>& f)
  {
    if(f)
    {
      (*this)(*f);
    }
  }

  template <template <typename...> typename T, typename... Args>
  requires avnd::variant_ish<T<Args...>>
  void operator()(const T<Args...>& f)
  {
    visit([&](const auto& arg) { (*this)(arg); }, f);
  }

  void operator()(const avnd::bitset_ish auto& v)
  {
    std::vector<ossia::value> res;
    res.resize(v.size());
    for(int i = 0; i < v.size(); i++)
      res[i] = v.test(i);
    val = std::move(res);
  }

  void operator()(const avnd::vector_ish auto& f)
  {
    std::vector<ossia::value> v;
    v.resize(f.size());
    for(int i = 0, n = f.size(); i < n; i++)
      to_ossia_value_impl{v[i]}(f[i]);
    val = std::move(v);
  }

  void operator()(const oscr::type_wrapper auto& f)
  {
    auto& [obj] = f;
    (*this)(obj);
  }

  void operator()(const avnd::set_ish auto& f)
  {
    std::vector<ossia::value> v;
    v.resize(f.size());
    std::size_t i = 0;
    for(auto& set_element : f)
    {
      to_ossia_value_impl{v[i]}(set_element);
      i++;
    }
    val = std::move(v);
  }

  template<avnd::map_ish T>
  void operator()(const T& f)
  {
    using key_type = typename T::key_type;
    if constexpr(std::is_convertible_v<key_type, std::string>)
    {
      ossia::value_map_type v;
      v.reserve(f.size());

      for(const auto& [map_k, map_v] : f)
      {
        ossia::value sub;
        to_ossia_value_impl{sub}(map_v);

        v.emplace_back(map_k, std::move(sub));
      }
      val = std::move(v);
    }
    else
    {
      std::vector<ossia::value> v;
      v.reserve(f.size());

      for(auto& [map_k, map_v] : f)
      {
        std::vector<ossia::value> sub(2);

        to_ossia_value_impl{sub[0]}(map_k);
        to_ossia_value_impl{sub[1]}(map_v);

        v.push_back(std::move(sub));
      }
      val = std::move(v);
    }
  }

  template <std::floating_point T>
  void operator()(const T (&f)[2])
  {
    val = ossia::vec2f{f[0], f[1]};
  }

  template <std::floating_point T>
  void operator()(const T (&f)[3])
  {
    val = ossia::vec3f{f[0], f[1], f[2]};
  }

  template <std::floating_point T>
  void operator()(const T (&f)[4])
  {
    val = ossia::vec4f{f[0], f[1], f[2], f[3]};
  }

  void operator()(const auto& f) = delete;
  //{
  //  val = f;
  //}
};
template <typename T>
ossia::value to_ossia_value_rec(T&& v)
{
  //static_assert(std::is_void_v<T>, "unsupported case");
  ossia::value val;
  to_ossia_value_impl{val}(v);
  return val;
}

template <typename T>
ossia::value to_ossia_value(const T& v)
{
  using type = std::decay_t<T>;
  constexpr int sz = avnd::pfr::tuple_size_v<type>;
  if constexpr(sz == 0)
  {
    return ossia::impulse{};
  }
  else if constexpr(vecf_compatible<type>())
  {
    if constexpr(sz == 2)
    {
      auto [x, y] = v;
      return ossia::vec2f{x, y};
    }
    else if constexpr(sz == 3)
    {
      auto [x, y, z] = v;
      return ossia::vec3f{x, y, z};
    }
    else if constexpr(sz == 4)
    {
      auto [x, y, z, w] = v;
      return ossia::vec4f{x, y, z, w};
    }
    else
    {
      return to_ossia_value_rec(v);
    }
  }
  else
  {
    return to_ossia_value_rec(v);
  }
}

template <typename T, std::size_t N>
requires avnd::array_ish<T, N> &&(!avnd::vector_ish<T>)ossia::value
    to_ossia_value(const T& v)
{
  if constexpr(std::is_floating_point_v<T>)
  {
    if constexpr(N == 2)
    {
      auto [x, y] = v;
      return ossia::vec2f{x, y};
    }
    else if constexpr(N == 3)
    {
      auto [x, y, z] = v;
      return ossia::vec3f{x, y, z};
    }
    else if constexpr(N == 4)
    {
      auto [x, y, z, w] = v;
      return ossia::vec4f{x, y, z, w};
    }
    else
    {
      return std::vector<ossia::value>(std::begin(v), std::end(v));
    }
  }
  else if constexpr(std::is_integral_v<T>)
  {
    if constexpr(N == 2)
    {
      auto [x, y] = v;
      return ossia::vec2f{(float)x, (float)y};
    }
    else if constexpr(N == 3)
    {
      auto [x, y, z] = v;
      return ossia::vec3f{(float)x, (float)y, (float)z};
    }
    else if constexpr(N == 4)
    {
      auto [x, y, z, w] = v;
      return ossia::vec4f{(float)x, (float)y, (float)z, (float)w};
    }
    else
    {
      return std::vector<ossia::value>(std::begin(v), std::end(v));
    }
  }
  else
  {
    return std::vector<ossia::value>(std::begin(v), std::end(v));
  }
}

ossia::value to_ossia_value(const avnd::bitset_ish auto& v)
{
  std::vector<ossia::value> res;
  res.resize(v.size());
  for(int i = 0; i < v.size(); i++)
    res[i] = v.test(i);
  return res;
}

ossia::value to_ossia_value(const std::integral auto& v)
{
  return v;
}
ossia::value to_ossia_value(const std::floating_point auto& v)
{
  return v;
}
ossia::value to_ossia_value(const avnd::variant_ish auto& v)
{
  return to_ossia_value_rec(v);
}
ossia::value to_ossia_value(const avnd::optional_ish auto& v)
{
  return to_ossia_value_rec(v);
}
ossia::value to_ossia_value(const avnd::set_ish auto& v)
{
  return to_ossia_value_rec(v);
}
ossia::value to_ossia_value(const avnd::map_ish auto& v)
{
  return to_ossia_value_rec(v);
}

template <avnd::vector_ish T>
requires(!avnd::string_ish<T>) ossia::value to_ossia_value(const T& v)
{
  return to_ossia_value_rec(v);
}
ossia::value to_ossia_value(const oscr::type_wrapper auto& v)
{
  auto& [obj] = v;
  return to_ossia_value_rec(obj);
}
ossia::value to_ossia_value(const avnd::string_ish auto& v)
{
  return std::string{v};
}
ossia::value to_ossia_value(const avnd::enum_ish auto& v)
{
  return static_cast<int>(v);
}
template <typename T>
requires std::is_enum_v<T> ossia::value to_ossia_value(const T& v)
{
  return static_cast<int>(v);
}

inline ossia::value to_ossia_value(const bool& v)
{
  return v;
}

inline ossia::value to_ossia_value(auto& field, const auto& src)
{
  return to_ossia_value(src);
}
}
