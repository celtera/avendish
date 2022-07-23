#pragma once
#include <avnd/concepts/parameter.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/binding/ossia/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

namespace oscr
{
// For interleaving / deinterleaving fft:
// https://stackoverflow.com/a/55112294/1495627
static constexpr int source_index(int i, int length)
{
  const int mid = length - length / 2;
  if (i<mid)
    return i * 2;
  else
    return (i-mid) * 2 + 1;
}

template<typename T>
constexpr void deinterleave(T* arr, int length)
{
  if(length <= 1)
    return;

  for(int i = 1; i < length; i++)
  {
    int j = source_index(i, length);

    while (j < i) {
      j = source_index(j, length);
    }

    std::swap(arr[i], arr[j]);
  }
}

template<typename T>
struct is_vector_ish : std::false_type { };
template<avnd::vector_ish T>
requires (!avnd::string_ish<T>)
struct is_vector_ish<T> : std::true_type { };

template<typename T>
struct is_vector_v_ish : std::false_type { using value_type = void; };

template<>
struct is_vector_v_ish<std::string> : std::false_type { using value_type = void; };

template<typename T>
requires avnd::vector_v_ish<T, float>
struct is_vector_v_ish<T> : std::true_type { using value_type = float; };
template<typename T>
requires avnd::vector_v_ish<T, double>
struct is_vector_v_ish<T> : std::true_type { using value_type = double; };
template<typename T>
requires avnd::vector_v_ish<T, int8_t>
struct is_vector_v_ish<T> : std::true_type { using value_type = int8_t; };
template<typename T>
requires avnd::vector_v_ish<T, int16_t>
struct is_vector_v_ish<T> : std::true_type { using value_type = int16_t; };
template<typename T>
requires avnd::vector_v_ish<T, int32_t>
struct is_vector_v_ish<T> : std::true_type { using value_type = int32_t; };
template<typename T>
requires avnd::vector_v_ish<T, int64_t>
struct is_vector_v_ish<T> : std::true_type { using value_type = int64_t; };
template<typename T>
requires avnd::vector_v_ish<T, uint8_t>
struct is_vector_v_ish<T> : std::true_type { using value_type = uint8_t; };
template<typename T>
requires avnd::vector_v_ish<T, uint16_t>
struct is_vector_v_ish<T> : std::true_type { using value_type = uint16_t; };
template<typename T>
requires avnd::vector_v_ish<T, uint32_t>
struct is_vector_v_ish<T> : std::true_type { using value_type = uint32_t; };
template<typename T>
requires avnd::vector_v_ish<T, uint64_t>
struct is_vector_v_ish<T> : std::true_type { using value_type = uint64_t; };

struct from_ossia_value_impl
{
  template<typename F>
  bool from_vector(const ossia::value& src, F& f)
  {
    if(auto ptr = src.target<std::vector<ossia::value>>())
    {
      const std::vector<ossia::value>& v = *ptr;

      int k = 0;
      avnd::pfr::for_each_field(f, [&] (auto& f) {
        if(k < v.size())
          from_ossia_value_impl{}(v[k++], f);
      });
      return true;
    }
    else
    {
      return false;
    }
  }

  template<typename F>
  requires std::is_aggregate_v<F>
  bool operator()(const ossia::value& src, F& dst)
  {
    constexpr int sz = avnd::pfr::tuple_size_v<F>;
    if constexpr(vecf_compatible<F>())
    {
      if constexpr (sz == 2)
      {
        auto [x, y] = ossia::convert<ossia::vec2f>(src);
        dst = {x, y};
        return true;
      }
      else if constexpr (sz == 3)
      {
        auto [x, y, z] = ossia::convert<ossia::vec3f>(src);
        dst = {x, y, z};
        return true;
      }
      else if constexpr (sz == 4)
      {
        auto [x, y, z, w] = ossia::convert<ossia::vec4f>(src);
        dst = {x, y, z, w};
        return true;
      }
      else
      {
        return from_vector(src, dst);
      }
    }
    else
    {
      return from_vector(src, dst);
    }
  }

  template<typename F>
  requires (type_wrapper<F>)
  bool operator()(const ossia::value& src, F& dst)
  {
    auto& [obj] = dst;
    (*this)(src, obj);
    return true;
  }

  template<typename F>
  requires (avnd::optional_ish<F>)
  bool operator()(const ossia::value& src, F& dst)
  {
    dst = F{std::in_place};
    return true;
  }

  template<typename F>
  requires (std::is_integral_v<F>)
  bool operator()(const ossia::value& src, F& f)
  {
    f = ossia::convert<int>(src);
    return true;
  }

  template<typename F>
  requires (std::is_floating_point_v<F>)
  bool operator()(const ossia::value& src, F& f)
  {
    f = ossia::convert<float>(src);
    return true;
  }

  bool operator()(const ossia::value& src, bool& f)
  {
    f = ossia::convert<bool>(src);
    return true;
  }

  bool operator()(const ossia::value& src, std::vector<bool>::reference f)
  {
    f = ossia::convert<bool>(src);
    return true;
  }

  bool operator()(const ossia::value& src, std::string& f)
  {
    f = ossia::convert<std::string>(src);
    return true;
  }

  template<template<typename> typename Pred, typename Internal, template<typename...> typename T, typename... Args>
  bool assign_if_matches_predicate(const ossia::value& src, T<Args...>& f)
  {
    using namespace boost::mp11;
    using index = mp_find_if<T<Args...>, Pred>;
    if constexpr(!std::is_same_v<index, mp_size<T<Args...>>>) {
      using type = mp_at<T<Args...>, index>;
      f = (type)*src.target<Internal>();
      return true;
    }
    else {
      return false;
    }
  }

  template<template<typename> typename Pred, template<typename...> typename T, typename... Args>
  bool assign_if_matches_predicate_vec(const ossia::value& src, T<Args...>& f)
  {
    // FIXME If Args... has a type which looks like a vector of variants...
    using namespace boost::mp11;
    using index = mp_find_if<T<Args...>, Pred>;
    if constexpr(!std::is_same_v<index, mp_size<T<Args...>>>) {
      using type = mp_at<T<Args...>, index>;

      type t;
      (*this)(src, t);
      f = std::move(t);

      return true;
    }
    else {
      return false;
    }
  }

  template <template<typename...> typename T, typename... Args>
  requires avnd::variant_ish<T<Args...>>
  bool operator()(const ossia::value& src, T<Args...>& f)
  {
    using namespace boost::mp11;
    switch(src.get_type())
    {
      case ossia::val_type::FLOAT:
      {
        if constexpr(std::is_constructible_v<T<Args...>, float>)
        {
          f = *src.target<float>();
          return true;
        }
        else if constexpr(std::is_constructible_v<T<Args...>, double>)
        {
          f = (double)*src.target<float>();
          return true;
        }
        else
        {
          return assign_if_matches_predicate<std::is_integral, float>(src, f);
        }
        break;
      }
      case ossia::val_type::INT:
      {
        if constexpr(std::is_constructible_v<T<Args...>, int>)
        {
          f = *src.target<int>();
          return true;
        }
        else
        {
          if(assign_if_matches_predicate<std::is_integral, int>(src, f))
            return true;
          else if(assign_if_matches_predicate<std::is_floating_point, int>(src, f))
            return true;
          else
            return false;
        }
        break;
      }
      case ossia::val_type::VEC2F:
      {
        // FIXME If Args... has a type which looks like a vector of float...
        break;
      }
      case ossia::val_type::VEC3F:
      {
        break;
      }
      case ossia::val_type::VEC4F:
      {
        break;
      }
      case ossia::val_type::IMPULSE:
      {
        f = {};
        break;
      }
      case ossia::val_type::BOOL:
      {
        if constexpr(std::is_constructible_v<T<Args...>, bool>)
        {
          f = *src.target<bool>();
          return true;
        }
        else
        {
          if(assign_if_matches_predicate<std::is_integral, bool>(src, f))
            return true;
          else if(assign_if_matches_predicate<std::is_floating_point, bool>(src, f))
            return true;
          else
            return false;
        }
        break;
      }
      case ossia::val_type::STRING:
      {
        if constexpr(std::is_constructible_v<T<Args...>, std::string>)
        {
          f = *src.target<std::string>();
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::LIST:
      {
        if(assign_if_matches_predicate_vec<is_vector_ish>(src, f))
          return true;
        else if(assign_if_matches_predicate_vec<is_vector_v_ish>(src, f))
          return true;
        else
          return false;
        break;
      }
      case ossia::val_type::CHAR:
      {
        break;
      }
    }
    return false;
  }

  template<avnd::vector_ish T>
  requires (!avnd::string_ish<T>)
  bool operator()(const ossia::value& src, T& f)
  {
    switch(src.get_type())
    {
      case ossia::val_type::VEC2F:
      {
        if constexpr(std::is_constructible_v<typename T::value_type, float>)
        {
          ossia::vec2f v = *src.target<ossia::vec2f>();
          f.assign(v.begin(), v.end());
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::VEC3F:
      {
        if constexpr(std::is_constructible_v<typename T::value_type, float>)
        {
          ossia::vec3f v = *src.target<ossia::vec3f>();
          f.assign(v.begin(), v.end());
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::VEC4F:
      {
        if constexpr(std::is_constructible_v<typename T::value_type, float>)
        {
          ossia::vec4f v = *src.target<ossia::vec4f>();
          f.assign(v.begin(), v.end());
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::LIST:
      {
        const std::vector<ossia::value>& vl = *src.target<std::vector<ossia::value>>();
        f.resize(vl.size());
        for(int i = 0, n = vl.size(); i < n; i++)
          (*this)(vl[i], f[i]);
        return true;
        break;
      }
      default:
        return false;
        break;
    }
  }

  template<std::size_t N, typename T>
  bool from_array(const ossia::value& src, T& f)
  {
    using value_type = std::remove_const_t<std::remove_reference_t<std::decay_t<decltype(f[0])>>>;
    switch(src.get_type())
    {
      case ossia::val_type::VEC2F:
      {
        if constexpr(std::is_constructible_v<value_type, float>)
        {
          ossia::vec2f v = *src.target<ossia::vec2f>();
          for(int i = 0, n = std::min(std::size_t(2), N); i < n; i++)
            f[i] = v[i];
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::VEC3F:
      {
        if constexpr(std::is_constructible_v<value_type, float>)
        {
          ossia::vec3f v = *src.target<ossia::vec3f>();
          for(int i = 0, n = std::min(std::size_t(3), N); i < n; i++)
            f[i] = v[i];
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::VEC4F:
      {
        if constexpr(std::is_constructible_v<value_type, float>)
        {
          ossia::vec4f v = *src.target<ossia::vec4f>();
          for(int i = 0, n = std::min(std::size_t(4), N); i < n; i++)
            f[i] = v[i];
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::LIST:
      {
        const std::vector<ossia::value>& vl = *src.target<std::vector<ossia::value>>();
        for(int i = 0, n = std::min(vl.size(), N); i < n; i++)
          (*this)(vl[i], f[i]);
        return true;
        break;
      }
      default:
        return false;
        break;
    }
  }

  template<std::size_t N, typename T>
  bool operator()(const ossia::value& src, T(&f)[N])
  {
    return from_array<N>(src, f);
  }

  template<std::size_t N, typename T>
  bool operator()(const ossia::value& src, std::array<T, N>& f)
  {
    return from_array<N>(src, f);
  }

  template <template<typename, std::size_t, typename...> typename T, typename Val, std::size_t N>
  requires avnd::array_ish<T<Val, N>, N> && (!avnd::string_ish<T<Val, N>>) && (!avnd::vector_ish<T<Val, N>>)
  bool operator()(const ossia::value& src, T<Val, N>& f)
  {
    return from_array<N>(src, f);
  }

  template <template<std::size_t, typename...> typename T, std::size_t N>
  requires avnd::array_ish<T<N>, N> && (!avnd::string_ish<T<N>>) && (!avnd::vector_ish<T<N>>) && (!avnd::bitset_ish<T<N>>)
  bool operator()(const ossia::value& src, T<N>& f)
  {
    return from_array<N>(src, f);
  }

  bool operator()(const ossia::value& src, avnd::bitset_ish auto& f)
  {
    auto vec = src.target<std::vector<ossia::value>>();
    if(!vec)
      return false;

    if constexpr(requires { f.resize(123); }) {
      f.resize(vec->size());
    }

    f.reset();
    for(int i = 0; i < std::min(f.size(), vec->size()); i++) {
      f.set(i, ossia::convert<bool>((*vec)[i]));
    }
    return true;
  }

  template<avnd::set_ish T>
  bool operator()(const ossia::value& src, T& f)
  {
    switch(src.get_type())
    {
      case ossia::val_type::VEC2F:
      {
        if constexpr(std::is_constructible_v<typename T::value_type, float>)
        {
          ossia::vec2f v = *src.target<ossia::vec2f>();
          f.clear();
          f.insert(v.begin(), v.end());
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::VEC3F:
      {
        if constexpr(std::is_constructible_v<typename T::value_type, float>)
        {
          ossia::vec3f v = *src.target<ossia::vec3f>();
          f.clear();
          f.insert(v.begin(), v.end());
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::VEC4F:
      {
        if constexpr(std::is_constructible_v<typename T::value_type, float>)
        {
          ossia::vec4f v = *src.target<ossia::vec4f>();
          f.clear();
          f.insert(v.begin(), v.end());
          return true;
        }
        else
        {
          return false;
        }
        break;
      }
      case ossia::val_type::LIST:
      {
        const std::vector<ossia::value>& vl = *src.target<std::vector<ossia::value>>();

        f.clear();
        if_possible(f.reserve(vl.size()));

        for(int i = 0, n = vl.size(); i < n; i++)
        {
          typename T::value_type val;

          (*this)(vl[i], val);

          f.insert(std::move(val));
        }
        return true;
        break;
      }
      default:
        return false;
        break;
    }
  }

  template<avnd::map_ish T>
  bool operator()(const ossia::value& src, T& f)
  {
    if(auto ptr = src.target<std::vector<ossia::value>>())
    {
      const std::vector<ossia::value>& v = *ptr;

      f.clear();
      if_possible(f.reserve(v.size()));

      for(auto& subv : v)
      {
        if(auto sub = subv.target<std::vector<ossia::value>>())
        {
          const std::vector<ossia::value>& vv = *sub;
          if(vv.size() == 2)
          {
            typename T::key_type map_k;
            from_ossia_value_impl{}(vv[0], map_k);

            typename T::mapped_type map_v;
            from_ossia_value_impl{}(vv[1], map_v);

            f.emplace(std::move(map_k), std::move(map_v));
          }
        }
      }

      return true;
    }
    else
    {
      return false;
    }
  }
  /*
    template<typename T>
    requires requires (T t) { t = 0.f; }
    void operator()(const ossia::value& src, T (&f)[2])
    { val = ossia::vec2f{f[0], f[1]}; }

  template<typename T>
  requires requires (T t) { t = 0.1f; }
  void operator()(const ossia::value& src, T (&f)[3])
  { val = ossia::vec3f{f[0], f[1], f[2]}; }

  template<typename T>
  requires requires (T t) { t = 0.1f; }
  void operator()(const ossia::value& src, T (&f)[4])
  { val = ossia::vec4f{f[0], f[1], f[2], f[3]}; }

  void operator()(const auto& f)
  {
    val = f;
  }*/
};

template <typename T>
bool from_ossia_value_rec(const ossia::value& src, T& dst)
{
  return from_ossia_value_impl{}(src, dst);
}


template <typename T>
bool from_ossia_value(const ossia::value& src, T& dst)
{
  using type = std::decay_t<T>;
  static_assert(!oscr::type_wrapper<T>);
  constexpr int sz = avnd::pfr::tuple_size_v<T>;
  if constexpr (sz == 0)
  {
    // Impulse case, nothing to do
    // FIXME unless we have an optional parameter !
    return true;
  }
  else if constexpr(vecf_compatible<type>())
  {
    if constexpr (sz == 2)
    {
      auto [x, y] = ossia::convert<ossia::vec2f>(src);
      dst = {x, y};
      return true;
    }
    else if constexpr (sz == 3)
    {
      auto [x, y, z] = ossia::convert<ossia::vec3f>(src);
      dst = {x, y, z};
      return true;
    }
    else if constexpr (sz == 4)
    {
      auto [x, y, z, w] = ossia::convert<ossia::vec4f>(src);
      dst = {x, y, z, w};
      return true;
    }
    else
    {
      return from_ossia_value_rec(src, dst);
    }
  }
  else
  {
    return from_ossia_value_rec(src, dst);
  }
}

template <std::integral T>
bool from_ossia_value(const ossia::value& src, T& dst)
{
  dst = ossia::convert<int>(src);
  return true;
}
template <std::floating_point T>
bool from_ossia_value(const ossia::value& src, T& dst)
{
  dst = ossia::convert<float>(src);
  return true;
}
template <avnd::string_ish T>
bool from_ossia_value(const ossia::value& src, T& dst)
{
  dst = ossia::convert<std::string>(src);
  return true;
}
inline bool from_ossia_value(const ossia::value& src, const char*& dst)
{
  if(auto p = src.target<std::string>())
  {
    dst = p->data();
    return true;
  }
  else
  {
    dst = "";
    return false;
  }
}
template <avnd::vector_ish T>
requires (!avnd::string_ish<T>)
bool from_ossia_value(const ossia::value& src, T& dst)
{
  return from_ossia_value_impl{}(src, dst);
}

template <template<typename, std::size_t, typename...> typename T, typename Val, std::size_t N>
requires avnd::array_ish<T<Val, N>, N> && (!avnd::string_ish<T<Val, N>>)
bool from_ossia_value(const ossia::value& src, T<Val, N>& dst)
{
  return from_ossia_value_impl{}(src, dst);
}

template <template<std::size_t, typename...> typename T, std::size_t N>
requires avnd::array_ish<T<N>, N> && (!avnd::string_ish<T<N>>)
    bool from_ossia_value(const ossia::value& src, T<N>& dst)
{
  return from_ossia_value_impl{}(src, dst);
}

template <avnd::variant_ish T>
bool from_ossia_value(const ossia::value& src, T& dst)
{
  return from_ossia_value_impl{}(src, dst);
}

template <avnd::set_ish T>
bool from_ossia_value(const ossia::value& src, T& dst)
{
  return from_ossia_value_impl{}(src, dst);
}

template <avnd::map_ish T>
bool from_ossia_value(const ossia::value& src, T& dst)
{
  return from_ossia_value_impl{}(src, dst);
}

template <avnd::bitset_ish T>
bool from_ossia_value(const ossia::value& src, T& dst)
{
  return from_ossia_value_impl{}(src, dst);
}

template <avnd::optional_ish T>
void from_ossia_value(const ossia::value& src, T& dst)
{
  using value_type = typename T::value_type;
  if constexpr(std::is_empty_v<value_type>)
  {
    dst = T{std::in_place};
  }
  else
  {
    value_type val;
    if(from_ossia_value(src, val))
      dst.emplace(std::move(val));
  }
}

template <oscr::type_wrapper T>
bool from_ossia_value(const ossia::value& src, T& dst)
{
  auto& [obj] = dst;
  from_ossia_value(src, obj);
  return true;
}

inline bool from_ossia_value(const ossia::value& src, bool& dst)
{
  switch(src.get_type()) {
    case ossia::val_type::BOOL:
      dst = *src.target<bool>();
      return true;
      break;
    default:
      return false;
      break;
  }
}

inline void from_ossia_value(auto& field, const ossia::value& src, auto& dst)
{
  from_ossia_value(src, dst);
}

template<avnd::enum_ish_parameter Field, typename Val>
struct enum_from_ossia_visitor
{
  Val operator()(const int& v) const noexcept {
    constexpr auto range = avnd::get_range<Field>();
    static_assert(std::size(range.values) > 0);
    if constexpr(requires (Val v) { v = range.values[0].second; })
    {
      if(v >= 0 && v < std::size(range.values))
        return range.values[v].second;

      return range.values[0].second;
    }
    else if constexpr(requires (Val v) { v = range.values[0]; })
    {
      if(v >= 0 && v < std::size(range.values))
        return range.values[v];

      return range.values[0];
    }
    else
    {
      return static_cast<Val>(v);
    }
  }
  Val operator()(const std::string& v) const noexcept {
    constexpr auto range = avnd::get_range<Field>();
    for(int i = 0; i < std::size(range.values); i++)
    {
      if constexpr(requires { v == range.values[i].first; })
      {
        if(v == range.values[i].first)
          return (*this)((int)i);
      }
      else
      {
        if(v == range.values[i])
          return (*this)((int)i);
      }
    }
    return (*this)(0);
  }
  Val operator()(const auto& v) const noexcept { return Val{}; }
  Val operator()() const noexcept { return Val{}; }
};

template<avnd::enum_ish_parameter Field, typename Val>
inline void from_ossia_value(
    Field& field,
    const ossia::value& src,
    Val& dst)
{
  if constexpr(avnd::enum_parameter<Field>)
  {
    dst = src.apply(enum_from_ossia_visitor<Field, Val>{});
  }
  else
  {
    // In score we already get the correct value corresponding to the
    // choosen element in the combobox so we just have to unpack it:
    from_ossia_value(src, dst);
  }
}

}
