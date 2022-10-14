#pragma once
#include <avnd/binding/ossia/value.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/parameter.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <boost/mp11/algorithm.hpp>
#include <type_traits>
namespace oscr
{
// For interleaving / deinterleaving fft:
// https://stackoverflow.com/a/55112294/1495627
static constexpr int source_index(int i, int length)
{
  const int mid = length - length / 2;
  if(i < mid)
    return i * 2;
  else
    return (i - mid) * 2 + 1;
}

template <typename T>
constexpr void deinterleave(T* arr, int length)
{
  if(length <= 1)
    return;

  for(int i = 1; i < length; i++)
  {
    int j = source_index(i, length);

    while(j < i)
    {
      j = source_index(j, length);
    }

    std::swap(arr[i], arr[j]);
  }
}

template <typename T>
struct is_vector_ish : std::false_type
{
};
template <avnd::vector_ish T>
requires(!avnd::string_ish<T>) struct is_vector_ish<T> : std::true_type
{
};

template <typename T>
struct is_map_ish : std::false_type
{
};
template <avnd::map_ish T>
struct is_map_ish<T> : std::true_type
{
};

template<typename T>
concept int_compatible = std::is_constructible_v<T, int> && std::is_arithmetic_v<T>;

template<typename T>
concept float_compatible = std::is_constructible_v<T, float> && std::is_arithmetic_v<T>;

template<typename T>
concept double_compatible = std::is_constructible_v<T, double> && std::is_arithmetic_v<T>;

template <template<typename...> typename Pred, typename T>
struct predicate_vector : std::false_type
{
  using value_type = void;
};

template <template<typename...> typename Pred, typename T>
requires Pred<typename T::value_type>::value
struct predicate_vector<Pred, T> : std::integral_constant<bool, Pred<typename T::value_type>::value>
{
  using value_type = std::conditional_t<Pred<typename T::value_type>::value, typename T::value_type, void>;
};

template <template<typename...> typename Pred, typename T>
struct predicate_map : std::false_type
{
  using value_type = void;
};

template <template<typename...> typename Pred, typename T>
requires Pred<typename T::mapped_type>::value
struct predicate_map<Pred, T> : std::integral_constant<bool, Pred<typename T::mapped_type>::value>
{
  using value_type = std::conditional_t<Pred<typename T::mapped_type>::value, typename T::mapped_type, void>;
};


// Used to lookup types similar to ossia's in variants
template <typename T>
using is_float = std::is_same<float, T>;
// FIXME find better ways
template <typename T>
using is_vec2f = std::integral_constant<bool, requires (T t) { T{0.f, 0.f}; } && sizeof(T) == 2 * sizeof(float)>;
template <typename T>
using is_vec3f = std::integral_constant<bool, requires (T t) { T{0.f, 0.f, 0.f}; }  && sizeof(T) == 3 * sizeof(float)>;
template <typename T>
using is_vec4f = std::integral_constant<bool, requires (T t) { T{0.f, 0.f, 0.f, 0.f}; } && sizeof(T) == 4 * sizeof(float)>;
template <typename T>
using is_vecNf = std::integral_constant<bool, avnd::vector_v_strict<T, float>>;
template <typename T>
using is_vec2d = std::integral_constant<bool, requires (T t) { T{0., 0.}; } && sizeof(T) == 2 * sizeof(double)>;
template <typename T>
using is_vec3d = std::integral_constant<bool, requires (T t) { T{0., 0., 0.}; }  && sizeof(T) == 3 * sizeof(double)>;
template <typename T>
using is_vec4d = std::integral_constant<bool, requires (T t) { T{0., 0., 0., 0.}; } && sizeof(T) == 4 * sizeof(double)>;
template <typename T>
using is_vecNd = std::integral_constant<bool, avnd::vector_v_strict<T, double>>;
template <typename T>
using is_int = std::is_same<int, T>;
template<typename T>
using is_large_enough_int = std::integral_constant<bool, std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) >= 4>;
template <typename T>
using is_string = std::is_same<std::string, T>;
template <typename T>
using is_bool = std::is_same<bool, T>;
template <typename T>
using is_variant = std::integral_constant<bool, avnd::variant_ish<T> || oscr::type_wrapper<T>>;

// Exclude string-ish stuff
template<typename T>
using is_number = std::conjunction<std::is_arithmetic<T>, std::negation<std::is_same<T, char>>>;
template <typename T>
using is_number_vector = predicate_vector<is_number, T>;

// to lookup vector<T>'s that match an std::vector<ossia::value> closely
template <typename T>
using is_fp_vector = predicate_vector<std::is_floating_point, T>;

template <typename T>
using is_float_vector = predicate_vector<is_float, T>;
template <typename T>
using is_int_vector = predicate_vector<is_int, T>;
template <typename T>
using is_vec2f_vector = predicate_vector<is_vec2f, T>;
template <typename T>
using is_vec3f_vector = predicate_vector<is_vec3f, T>;
template <typename T>
using is_vec4f_vector = predicate_vector<is_vec4f, T>;
template <typename T>
using is_vecNf_vector = predicate_vector<is_vecNf, T>;
template <typename T>
using is_vec2d_vector = predicate_vector<is_vec2d, T>;
template <typename T>
using is_vec3d_vector = predicate_vector<is_vec3d, T>;
template <typename T>
using is_vec4d_vector = predicate_vector<is_vec4d, T>;
template <typename T>
using is_vecNd_vector = predicate_vector<is_vecNd, T>;
template <typename T>
using is_large_enough_int_vector = predicate_vector<is_large_enough_int, T>;
template <typename T>
using is_string_vector = predicate_vector<is_string, T>;
template <typename T>
using is_bool_vector = predicate_vector<is_bool, T>;
template <typename T>
using is_monostate_vector = predicate_vector<std::is_empty, T>;
template <typename T>
using is_variant_vector = predicate_vector<is_variant, T>;
template <typename T>
using is_variant_map = predicate_map<is_variant, T>; //FIXME
template <typename T>
using is_variant_list_vector = predicate_vector<is_variant_vector, T>;
template <typename T>
using is_variant_map_vector = predicate_vector<is_variant_map, T>;



template <typename T>
struct is_vector_v_ish : std::false_type
{
  using value_type = void;
};

template <>
struct is_vector_v_ish<std::string> : std::false_type
{
  using value_type = void;
};

template <typename T>
requires avnd::vector_v_ish<T, float>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = float;
};
template <typename T>
requires avnd::vector_v_ish<T, double>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = double;
};
template <typename T>
requires avnd::vector_v_ish<T, int8_t>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = int8_t;
};
template <typename T>
requires avnd::vector_v_ish<T, int16_t>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = int16_t;
};
template <typename T>
requires avnd::vector_v_ish<T, int32_t>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = int32_t;
};
template <typename T>
requires avnd::vector_v_ish<T, int64_t>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = int64_t;
};
template <typename T>
requires avnd::vector_v_ish<T, uint8_t>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = uint8_t;
};
template <typename T>
requires avnd::vector_v_ish<T, uint16_t>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = uint16_t;
};
template <typename T>
requires avnd::vector_v_ish<T, uint32_t>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = uint32_t;
};
template <typename T>
requires avnd::vector_v_ish<T, uint64_t>
struct is_vector_v_ish<T> : std::true_type
{
  using value_type = uint64_t;
};

struct from_ossia_value_impl
{
  template <typename F>
  bool from_vector(const ossia::value& src, F& f)
  {
    if(auto ptr = src.target<ossia::value_map_type>())
    {
      const auto& v = *ptr;

      auto it = v.begin();
      avnd::pfr::for_each_field(f, [&](auto& f) {
        if(it != v.end())
        {
          from_ossia_value_impl{}(it->second, f);
          it++;
        }
      });
      return true;
    }
    else if(auto ptr = src.target<std::vector<ossia::value>>())
    {
      const auto& v = *ptr;

      int k = 0;
      avnd::pfr::for_each_field(f, [&](auto& f) {
        if(k < v.size())
          from_ossia_value_impl{}(v[k++], f);
      });
      return true;
    }
    else if(auto ptr = src.target<ossia::vec2f>())
    {
      const auto& v = *ptr;

      int k = 0;
      avnd::pfr::for_each_field(f, [&](auto& f) {
        if(k < v.size())
          from_ossia_value_impl{}(v[k++], f);
      });
      return true;
    }
    else if(auto ptr = src.target<ossia::vec3f>())
    {
      const auto& v = *ptr;

      int k = 0;
      avnd::pfr::for_each_field(f, [&](auto& f) {
        if(k < v.size())
          from_ossia_value_impl{}(v[k++], f);
      });
      return true;
    }
    else if(auto ptr = src.target<ossia::vec4f>())
    {
      const auto& v = *ptr;

      int k = 0;
      avnd::pfr::for_each_field(f, [&](auto& f) {
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

  template <typename F>
  requires std::is_aggregate_v<F>
  bool operator()(const ossia::value& src, F& dst)
  {
    constexpr int sz = avnd::pfr::tuple_size_v<F>;
    if constexpr(vecf_compatible<F>())
    {
      if constexpr(sz == 2)
      {
        auto [x, y] = ossia::convert<ossia::vec2f>(src);
        dst = {x, y};
        return true;
      }
      else if constexpr(sz == 3)
      {
        auto [x, y, z] = ossia::convert<ossia::vec3f>(src);
        dst = {x, y, z};
        return true;
      }
      else if constexpr(sz == 4)
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

  template <typename F>
  requires(avnd::optional_ish<F>) bool operator()(const ossia::value& src, F& dst)
  {
    dst = F{std::in_place};
    return true;
  }

  template <typename F>
  requires(std::is_integral_v<F>) bool operator()(const ossia::value& src, F& f)
  {
    f = ossia::convert<int>(src);
    return true;
  }

  template <typename F>
  requires(std::is_floating_point_v<F>) bool operator()(const ossia::value& src, F& f)
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

  template <
      template <typename> typename Pred, typename Internal,
      template <typename...> typename T, typename... Args>
  bool assign_if_matches_predicate(const ossia::value& src, T<Args...>& f)
  {
    using namespace boost::mp11;
    using index = mp_find_if<T<Args...>, Pred>;
    if constexpr(!std::is_same_v<index, mp_size<T<Args...>>>)
    {
      using type = mp_at<T<Args...>, index>;
      f = (type)*src.target<Internal>();
      return true;
    }
    else
    {
      return false;
    }
  }

#define CHECK_PREDICATE(Pred) \
    using index = boost::mp11::mp_find_if<T<Args...>, Pred>; \
    (!std::is_same_v<index, sz>)

#define APPLY_VEC_PREDICATE \
  { \
    using var_type = boost::mp11::mp_at<T<Args...>, index>; \
    var_type t; t.resize(srclist.size()); \
    for(std::size_t k = 0; k < srclist.size(); k++) \
      (*this)(srclist[k], t[k]); \
    f = std::move(t); \
    return true; \
  }

#define APPLY_MAP_PREDICATE \
  { \
    using var_type = boost::mp11::mp_at<T<Args...>, index>; \
    var_type t; if_possible(t.reserve(srcmap.size())); \
    for(const auto& [k, v] : srcmap) { \
      typename var_type::mapped_type res_v; \
      (*this)(v, res_v); \
      t.emplace(k, std::move(res_v)); \
    } \
    f = std::move(t); \
    return true; \
  }

  template <template <typename...> typename T,
      typename... Args>
  bool variant_assign_vec_to_closest_value_type(ossia::val_type type, const std::vector<ossia::value>& srclist, T<Args...>& f)
  {
    using sz = boost::mp11::mp_size<T<Args...>>;
    switch(type)
    {
      case ossia::val_type::FLOAT:
      {
        // We likely have a std::vector<ossia::value> where the values are floats.
        // If there's a std::vector<float> type in the variant we use it in priority
        // If there's a std::vector<float-ish> type in the variant we use it
        // Otherwise we can downgrade to any number-like type
        if constexpr(CHECK_PREDICATE(is_float_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_fp_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_number_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_variant_vector))
          APPLY_VEC_PREDICATE
        else
          return false;
        break;
      }
      case ossia::val_type::INT:
      {
        if constexpr(CHECK_PREDICATE(is_int_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_large_enough_int_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_number_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_variant_vector))
          APPLY_VEC_PREDICATE
        else
          return false;
        break;
      }
      case ossia::val_type::VEC2F:
      {
        if constexpr(CHECK_PREDICATE(is_vec2f_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_vec2d_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_vecNf_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_vecNd_vector))
          APPLY_VEC_PREDICATE
            else if constexpr(CHECK_PREDICATE(is_variant_vector))
              APPLY_VEC_PREDICATE
        else // FIXME maybe if the type has a generic assignable variant-ihs data type?
            // FIXME or something like a matrix type, e.g. vector<vector<float>> ?
          return false;
        break;
      }
      case ossia::val_type::VEC3F:
      {
        if constexpr(CHECK_PREDICATE(is_vec3f_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_vec3d_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_vecNf_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_vecNd_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_variant_vector))
          APPLY_VEC_PREDICATE
        else // FIXME maybe if the type has a generic assignable variant-ihs data type?
          return false;
        break;
      }
      case ossia::val_type::VEC4F:
      {
        if constexpr(CHECK_PREDICATE(is_vec4f_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_vec4d_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_vecNf_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_vecNd_vector))
          APPLY_VEC_PREDICATE

            else if constexpr(CHECK_PREDICATE(is_variant_vector))
              APPLY_VEC_PREDICATE
        else // FIXME maybe if the type has a generic assignable variant-ihs data type?
          return false;
        break;
      }
      case ossia::val_type::IMPULSE:
      {
        // FIXME bitset?
        if constexpr(CHECK_PREDICATE(is_monostate_vector))
          APPLY_VEC_PREDICATE
            else if constexpr(CHECK_PREDICATE(is_variant_vector))
              APPLY_VEC_PREDICATE
        else
          return false;
        break;
      }
      case ossia::val_type::BOOL:
      {
        // FIXME bitset?
        if constexpr(CHECK_PREDICATE(is_bool_vector))
          APPLY_VEC_PREDICATE

            else if constexpr(CHECK_PREDICATE(is_variant_vector))
              APPLY_VEC_PREDICATE
        else
          return false;
        break;
      }
      case ossia::val_type::STRING:
      {
        // FIXME bitset?
        if constexpr(CHECK_PREDICATE(is_string_vector))
          APPLY_VEC_PREDICATE

            else if constexpr(CHECK_PREDICATE(is_variant_vector))
              APPLY_VEC_PREDICATE
        else
          return false;
        break;
      }
      case ossia::val_type::LIST:
      {
        // FIXME bitset?
        if constexpr(CHECK_PREDICATE(is_variant_list_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_variant_vector))
          APPLY_VEC_PREDICATE
        else
          return false;
        break;
      }
      case ossia::val_type::MAP:
      {
        // FIXME bitset?
        if constexpr(CHECK_PREDICATE(is_variant_map_vector))
          APPLY_VEC_PREDICATE
        else if constexpr(CHECK_PREDICATE(is_variant_vector))
          APPLY_VEC_PREDICATE
        else
          return false;
        break;
      }
      default:
        break;
    }

    /*
    // FIXME If Args... has a type which looks like a vector of variants...
    using namespace boost::mp11;
    using index = mp_find_if<T<Args...>, Pred>;
    if constexpr(!std::is_same_v<index, mp_size<T<Args...>>>)
    {
      using type = mp_at<T<Args...>, index>;

      type t;
      (*this)(src, t);
      f = std::move(t);

      return true;
    }
    else
    {
      return false;
    }
    */
  }

  bool operator()(const ossia::value& src, oscr::type_wrapper auto& f)
  {
    auto& [obj] = f;
    return (*this)(src, obj);
  }

  template <template <typename...> typename T, typename... Args>
  requires avnd::variant_ish<T<Args...>>
  bool operator()(const ossia::value& src, T<Args...>& f)
  {
    using sz = boost::mp11::mp_size<T<Args...>>;
    using namespace boost::mp11;
    switch(src.get_type())
    {
      // std::monostate for NONE?
      default:
        break;
      case ossia::val_type::FLOAT: {
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
      case ossia::val_type::INT: {
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
      case ossia::val_type::VEC2F: {
        // FIXME If Args... has a type which looks like a vector of float...
        break;
      }
      case ossia::val_type::VEC3F: {
        break;
      }
      case ossia::val_type::VEC4F: {
        break;
      }
      case ossia::val_type::IMPULSE: {
        f = {};
        break;
      }
      case ossia::val_type::BOOL: {
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
      case ossia::val_type::STRING: {
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
      case ossia::val_type::LIST: {
        // Input: a ossia::value which is a std::vector<ossia::value>
        // Output: std::variant<A, B, ...>
        // We need to find the variant member which is the closest to what we have
        // 1. If it's an empty ossia list: take the first one, it doesn't matter
        //    as we have no further information on the content.
        auto& srclist = *src.target<std::vector<ossia::value>>();
        if(srclist.empty())
        {
          if constexpr(CHECK_PREDICATE(is_vector_ish))
              APPLY_VEC_PREDICATE
          // FIXME else if(assign_if_matches_predicate_vec<is_vector_v_ish>(src, f))
          // FIXME   return true;
          else
            return false;
        }
        else
        {
          // Use the first 8 elements as a hint:
          // if they're all the same type we assume the rest follows
          ossia::val_type vtype = srclist.front().get_type();
          bool different_types = false;
          for(std::size_t i = 1; i < std::min(std::size_t(8), srclist.size()); i++)
          {
            auto t = srclist[i].get_type();
            if(t != vtype)
            {
              different_types = true;
              break;
            }
          }
          if(different_types)
          {
            if constexpr(CHECK_PREDICATE(is_vector_ish))
              APPLY_VEC_PREDICATE
            else
              return false;
          }
          else
          {
            return variant_assign_vec_to_closest_value_type(vtype, srclist, f);
          }
        }
        // if(variant_assign_if_matches_predicate_vec<is_vector_ish>(src, f))
        //   return true;
        // FIXME else if(assign_if_matches_predicate_vec<is_vector_v_ish>(src, f))
        // FIXME   return true;
        // else
        //   return false;
        break;
      }
      case ossia::val_type::MAP: {
        // FIXME do the specific cases, e.g. map<string, int>, etc
        auto& srcmap = *src.target<ossia::value_map_type>();
        if constexpr(CHECK_PREDICATE(is_map_ish))
            APPLY_MAP_PREDICATE
        else
          return false;
        break;
      }
    }
    return false;
  }

  template <avnd::vector_ish T>
  requires(!avnd::string_ish<T>) bool operator()(const ossia::value& src, T& f)
  {
    switch(src.get_type())
    {
      case ossia::val_type::VEC2F: {
        if constexpr(float_compatible<typename T::value_type>)
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
      case ossia::val_type::VEC3F: {
        if constexpr(float_compatible<typename T::value_type>)
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
      case ossia::val_type::VEC4F: {
        if constexpr(float_compatible<typename T::value_type>)
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
      case ossia::val_type::LIST: {
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

  template <std::size_t N, typename T>
  bool from_array(const ossia::value& src, T& f)
  {
    using value_type
        = std::remove_const_t<std::remove_reference_t<std::decay_t<decltype(f[0])>>>;
    switch(src.get_type())
    {
      case ossia::val_type::VEC2F: {
        if constexpr(float_compatible<value_type>)
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
      case ossia::val_type::VEC3F: {
        if constexpr(float_compatible<value_type>)
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
      case ossia::val_type::VEC4F: {
        if constexpr(float_compatible<value_type>)
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
      case ossia::val_type::LIST: {
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

  template <std::size_t N, typename T>
  bool operator()(const ossia::value& src, T (&f)[N])
  {
    return from_array<N>(src, f);
  }

  template <std::size_t N, typename T>
  bool operator()(const ossia::value& src, std::array<T, N>& f)
  {
    return from_array<N>(src, f);
  }

  template <
      template <typename, std::size_t, typename...> typename T, typename Val,
      std::size_t N>
  requires avnd::array_ish<T<Val, N>, N> &&(!avnd::string_ish<T<Val, N>>)&&(
      !avnd::vector_ish<T<Val, N>>)bool
  operator()(const ossia::value& src, T<Val, N>& f)
  {
    return from_array<N>(src, f);
  }

  template <template <std::size_t, typename...> typename T, std::size_t N>
  requires avnd::array_ish<T<N>, N> &&(!avnd::string_ish<T<N>>)&&(
      !avnd::vector_ish<T<N>>)&&(!avnd::bitset_ish<T<N>>)bool
  operator()(const ossia::value& src, T<N>& f)
  {
    return from_array<N>(src, f);
  }

  bool operator()(const ossia::value& src, avnd::bitset_ish auto& f)
  {
    auto vec = src.target<std::vector<ossia::value>>();
    if(!vec)
      return false;

    if constexpr(requires { f.resize(123); })
    {
      f.resize(vec->size());
    }

    f.reset();
    for(int i = 0; i < std::min(f.size(), vec->size()); i++)
    {
      f.set(i, ossia::convert<bool>((*vec)[i]));
    }
    return true;
  }

  template <avnd::set_ish T>
  bool operator()(const ossia::value& src, T& f)
  {
    switch(src.get_type())
    {
      case ossia::val_type::VEC2F: {
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
      case ossia::val_type::VEC3F: {
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
      case ossia::val_type::VEC4F: {
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
      case ossia::val_type::LIST: {
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

  template <avnd::map_ish T>
  bool operator()(const ossia::value& src, T& f)
  {
    if(auto ptr = src.target<ossia::value_map_type>())
    {
      const ossia::value_map_type& v = *ptr;

      f.clear();
      if_possible(f.reserve(v.size()));

      for(auto& [key, val] : v)
      {
        using target_key_type = typename T::key_type;
        using target_value_type = typename T::mapped_type;
        if constexpr(std::is_constructible_v<target_key_type, std::string>)
        {
          target_value_type map_v{};
          from_ossia_value_impl{}(val, map_v);

          f.emplace(key, std::move(map_v));
        }
        else
        {
          // Try to convert the map key
          target_key_type map_k{};
          from_ossia_value_impl{}(key, map_k);

          target_value_type map_v{};
          from_ossia_value_impl{}(val, map_v);

          f.emplace(std::move(map_k), std::move(map_v));
        }
      }

      return true;
    }
    else if(auto ptr = src.target<std::vector<ossia::value>>())
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
#undef CHECK_PREDICATE
#undef APPLY_VEC_PREDICATE
#undef APPLY_MAP_PREDICATE
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
  if constexpr(sz == 0)
  {
    // Impulse case, nothing to do
    // FIXME unless we have an optional parameter !
    return true;
  }
  else if constexpr(vecf_compatible<type>())
  {
    if constexpr(sz == 2)
    {
      auto [x, y] = ossia::convert<ossia::vec2f>(src);
      dst = {x, y};
      return true;
    }
    else if constexpr(sz == 3)
    {
      auto [x, y, z] = ossia::convert<ossia::vec3f>(src);
      dst = {x, y, z};
      return true;
    }
    else if constexpr(sz == 4)
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
requires(!avnd::string_ish<T>) bool from_ossia_value(const ossia::value& src, T& dst)
{
  return from_ossia_value_impl{}(src, dst);
}

template <
    template <typename, std::size_t, typename...> typename T, typename Val,
    std::size_t N>
requires avnd::array_ish<T<Val, N>, N> &&(
    !avnd::string_ish<
        T<Val, N>>)bool from_ossia_value(const ossia::value& src, T<Val, N>& dst)
{
  return from_ossia_value_impl{}(src, dst);
}

template <template <std::size_t, typename...> typename T, std::size_t N>
requires avnd::array_ish<T<N>, N> &&(!avnd::string_ish<T<N>>)bool from_ossia_value(
    const ossia::value& src, T<N>& dst)
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
  switch(src.get_type())
  {
    case ossia::val_type::BOOL:
      dst = *src.target<bool>();
      return true;
      break;
    default:
      return false;
      break;
  }
}

inline void from_ossia_value(auto& field, const ossia::value& src, ossia::value& dst)
{
  dst = src;
}

inline void from_ossia_value(auto& field, const ossia::value& src, auto& dst)
{
  from_ossia_value(src, dst);
}

template <avnd::enum_ish_parameter Field, typename Val>
struct enum_from_ossia_visitor
{
  Val operator()(const int& v) const noexcept
  {
    constexpr auto range = avnd::get_range<Field>();
    static_assert(std::size(range.values) > 0);
    if constexpr(requires(Val v) { v = range.values[0].second; })
    {
      if(v >= 0 && v < std::size(range.values))
        return range.values[v].second;

      return range.values[0].second;
    }
    else if constexpr(requires(Val v) { v = range.values[0]; })
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
  Val operator()(const std::string& v) const noexcept
  {
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

template <avnd::enum_ish_parameter Field, typename Val>
inline void from_ossia_value(Field& field, const ossia::value& src, Val& dst)
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
