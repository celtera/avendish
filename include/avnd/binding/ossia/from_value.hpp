#pragma once
#include <avnd/concepts/parameter.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/binding/ossia/value.hpp>

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
    void from_vector(const ossia::value& src, F& f)
    {
      if(auto ptr = src.target<std::vector<ossia::value>>())
      {
        const std::vector<ossia::value>& v = *ptr;

        int k = 0;
        avnd::pfr::for_each_field(f, [&] (auto& f) {
          if(k < v.size())
            from_ossia_value_impl{}(v[k++], f);
        });
      }
    }

    template<typename F>
    requires std::is_aggregate_v<F>
    void operator()(const ossia::value& src, F& dst)
    {
      constexpr int sz = avnd::pfr::tuple_size_v<F>;
      if constexpr(vecf_compatible<F>())
      {
        if constexpr (sz == 2)
        {
          auto [x, y] = ossia::convert<ossia::vec2f>(src);
          dst = {x, y};
        }
        else if constexpr (sz == 3)
        {
          auto [x, y, z] = ossia::convert<ossia::vec3f>(src);
          dst = {x, y, z};
        }
        else if constexpr (sz == 4)
        {
          auto [x, y, z, w] = ossia::convert<ossia::vec4f>(src);
          dst = {x, y, z, w};
        }
        else
        {
          return from_vector(src, dst);
        }
      }
      else
      {
        from_vector(src, dst);
      }
    }

    template<typename F>
    requires (type_wrapper<F>)
    void operator()(const ossia::value& src, F& dst)
    {
      auto& [obj] = dst;
      (*this)(src, obj);
    }
    template<typename F>
    requires (std::is_integral_v<F>)
    void operator()(const ossia::value& src, F& f)
    {
      f = ossia::convert<int>(src);
    }

    template<typename F>
    requires (std::is_floating_point_v<F>)
    void operator()(const ossia::value& src, F& f)
    {
      f = ossia::convert<float>(src);
    }

    void operator()(const ossia::value& src, bool& f)
    {
      f = ossia::convert<bool>(src);
    }

    void operator()(const ossia::value& src, std::string& f)
    {
      f = ossia::convert<std::string>(src);
    }

    template <template<typename...> typename T, typename... Args>
    requires avnd::variant_ish<T<Args...>>
    void operator()(const ossia::value& src, T<Args...>& f)
    {
      switch(src.get_type())
      {
        case ossia::val_type::FLOAT:
        {
          f = *src.target<float>();
          break;
        }
        case ossia::val_type::INT:
        {
          f = *src.target<int>();
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
          f = *src.target<bool>();
          break;
        }
        case ossia::val_type::STRING:
        {
          f = *src.target<std::string>();
          break;
        }
        case ossia::val_type::LIST:
        {
          // FIXME If Args... has a type which looks like a vector of variants...
          using namespace boost::mp11;
          using var_vec_index = mp_find_if<T<Args...>, is_vector_ish>;
          if constexpr(!std::is_same_v<var_vec_index, mp_size<T<Args...>>>) {
            using type = mp_at<T<Args...>, var_vec_index>;
            type t;
            (*this)(src, t);
            f = std::move(t);
          }
          else
          {
            using vec_index = mp_find_if<T<Args...>, is_vector_v_ish>;
            if constexpr(!std::is_same_v<vec_index, mp_size<T<Args...>>>) {
              using type = mp_at<T<Args...>, var_vec_index>;
              type t;
              (*this)(src, t);
              f = std::move(t);
            }
          }
          break;
        }
        case ossia::val_type::CHAR:
        {
          break;
        }
      }
    }

    template<avnd::vector_ish T>
    requires (!avnd::string_ish<T>)
    void operator()(const ossia::value& src, T& f)
    {
      switch(src.get_type())
      {
      case ossia::val_type::VEC2F:
      {
        ossia::vec2f v = *src.target<ossia::vec2f>();
        f.assign(v.begin(), v.end());
        break;
      }
      case ossia::val_type::VEC3F:
      {
        ossia::vec3f v = *src.target<ossia::vec3f>();
        f.assign(v.begin(), v.end());
        break;
      }
      case ossia::val_type::VEC4F:
      {
        ossia::vec4f v = *src.target<ossia::vec4f>();
        f.assign(v.begin(), v.end());
        break;
      }
      case ossia::val_type::LIST:
      {
        const std::vector<ossia::value>& vl = *src.target<std::vector<ossia::value>>();
        f.resize(vl.size());
        for(int i = 0, n = vl.size(); i < n; i++)
          (*this)(vl[i], f[i]);
        break;
      }
      default:
          break;
      }
    }

    void operator()(const ossia::value& src, avnd::map_ish auto& f)
    {
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
void from_ossia_value_rec(const ossia::value& src, T& dst)
{
  from_ossia_value_impl{}(src, dst);
}


template <typename T>
void from_ossia_value(const ossia::value& src, T& dst)
{
  using type = std::decay_t<T>;
  static_assert(!oscr::type_wrapper<T>);
  constexpr int sz = avnd::pfr::tuple_size_v<T>;
  if constexpr (sz == 0)
  {
    // Impulse case, nothing to do
    // FIXME unless we have an optional parameter !
  }
  else if constexpr(vecf_compatible<type>())
  {
    if constexpr (sz == 2)
    {
      auto [x, y] = ossia::convert<ossia::vec2f>(src);
      dst = {x, y};
    }
    else if constexpr (sz == 3)
    {
      auto [x, y, z] = ossia::convert<ossia::vec3f>(src);
      dst = {x, y, z};
    }
    else if constexpr (sz == 4)
    {
      auto [x, y, z, w] = ossia::convert<ossia::vec4f>(src);
      dst = {x, y, z, w};
    }
    else
    {
      return from_ossia_value_rec(src, dst);
    }
  }
  else
  {
    from_ossia_value_rec(src, dst);
  }
}

template <std::integral T>
void from_ossia_value(const ossia::value& src, T& dst)
{
  dst = ossia::convert<int>(src);
}
template <std::floating_point T>
void from_ossia_value(const ossia::value& src, T& dst)
{
  dst = ossia::convert<float>(src);
}
template <avnd::string_ish T>
void from_ossia_value(const ossia::value& src, T& dst)
{
  dst = ossia::convert<std::string>(src);
}
inline void from_ossia_value(const ossia::value& src, const char*& dst)
{
  if(auto p = src.target<std::string>())
    dst = p->data();
  else
    dst = "";
}
template <avnd::vector_ish T>
requires (!avnd::string_ish<T>)
void from_ossia_value(const ossia::value& src, T& dst)
{
  from_ossia_value_impl{}(src, dst);
}
template <avnd::variant_ish T>
void from_ossia_value(const ossia::value& src, T& dst)
{
  from_ossia_value_impl{}(src, dst);
}
template <oscr::type_wrapper T>
void from_ossia_value(const ossia::value& src, T& dst)
{
  auto& [obj] = dst;
  from_ossia_value(src, obj);
}

inline void from_ossia_value(const ossia::value& src, bool& dst)
{
  dst = ossia::convert<bool>(src);
}

inline void from_ossia_value(auto& field, const ossia::value& src, auto& dst)
{
  from_ossia_value(src, dst);
}

template<avnd::enum_ish_parameter Field, typename Val>
struct enum_to_ossia_visitor
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
  dst = src.apply(enum_to_ossia_visitor<Field, Val>{});
}

}
