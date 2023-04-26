#pragma once
#include <avnd/concepts/curve.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <ossia/editor/curve/curve_segment/easing_helpers.hpp>
#include <ossia/network/value/value.hpp>

#include <optional>
namespace oscr
{
struct convert_to_curve
{
  template <typename T>
  std::optional<T> make_point(const ossia::value& point)
  {
    const ossia::value* x{};
    const ossia::value* y{};
    if(auto map = point.target<ossia::value_map_type>())
    {
      for(auto& [k, v] : *map)
      {
        if(k == "x")
          x = &v;
        else if(k == "y")
          y = &v;
      }
    }
    else if(auto arr = point.target<ossia::vec2f>())
    {
      T t{};
      t.x = (*arr)[0];
      t.y = (*arr)[1];
      return t;
    }
    else if(auto arr = point.target<std::vector<ossia::value>>())
    {
      if(arr->size() == 2)
      {
        x = &(*arr)[0];
        y = &(*arr)[1];
      }
    }

    if(!x || !y)
      return std::nullopt;

    T t{};
    t.x = ossia::convert<double>(*x);
    t.y = ossia::convert<double>(*y);
    return t;
  }

  void to_map(auto& res, const ossia::value& map)
  {
    if(auto str = map.target<std::string>())
      ossia::make_easing(res, *str);
    else if(auto power = map.target<float>())
      res = ossia::easing::power<float>{*power};
  }

  void to_map(auto& res, float starty, float endy, const ossia::value& map)
  {
    if(auto str = map.target<std::string>())
      ossia::make_easing(res, *str, starty, endy);
    else if(auto power = map.target<float>())
      res = [starty, endy,
             e = ossia::easing::power<float>{*power}](float ratio) constexpr noexcept {
        return ossia::easing::ease{}(starty, endy, e(ratio));
      };
  }

  template <avnd::curve_segment_start_end T>
  auto make_map(T& t, const ossia::value* map)
  {
    if constexpr(requires { t.map; })
    {
      if(map)
      {
        to_map(t.map, *map);
      }

      if(!t.map)
      {
        t.map = ossia::easing::linear{};
      }
    }
    else if constexpr(requires { t.function; })
    {
      if(map)
      {
        to_map(t.function, *map);
      }

      if(!t.function)
      {
        t.function = ossia::easing::linear{};
      }
    }
    else if constexpr(requires { t.gamma; })
    {
      t.gamma = 1.;
      if(map)
        if(auto v = map->target<float>())
          t.gamma = *v;
    }
    else if constexpr(requires { t.power; })
    {
      t.power = 1.;
      if(map)
        if(auto v = map->target<float>())
          t.power = *v;
    }
  }

  template <avnd::curve_segment_float T>
    requires avnd::curve_segment_map_function<T>
  auto make_map(T& t, float starty, float endy, const ossia::value* map)
  {
    if constexpr(requires { t.map; })
    {
      if(map)
      {
        to_map(t.map, starty, endy, *map);
      }

      if(!t.map)
      {
        t.map = ossia::make_easing<ossia::easing::linear>(starty, endy);
      }
    }
    else if constexpr(requires { t.function; })
    {
      if(map)
      {
        to_map(t.function, starty, endy, *map);
      }

      if(!t.function)
      {
        t.function = ossia::make_easing<ossia::easing::linear>(starty, endy);
      }
    }
  }

  // { struct { float x, y; } start, end; /*optional: map/power/easing*/ }
  // map is [0; 1] -> [0; 1] if provided
  template <avnd::curve_segment_start_end T>
  std::optional<T> make_segment(
      const ossia::value& start, const ossia::value& end, const ossia::value* map)
  {
    T t{};
    using p1_type = decltype(T::start);
    using p2_type = decltype(T::end);
    if(auto p1 = make_point<p1_type>(start))
      t.start = *p1;
    else
      return std::nullopt;
    if(auto p2 = make_point<p2_type>(end))
      t.end = *p2;
    else
      return std::nullopt;

    make_map(t, map);
    return t;
  }

  // { float start, end; function map; }
  // map is [0; 1] -> [min; max] if provided
  template <avnd::curve_segment_float T>
    requires avnd::curve_segment_map_function<T>
  std::optional<T> make_segment(
      const ossia::value& start, const ossia::value& end, const ossia::value* map)
  {
    T t{};
    struct point
    {
      float x, y;
    };
    point startp, endp;
    if(auto p1 = make_point<point>(start))
      startp = *p1;
    else
      return std::nullopt;
    if(auto p2 = make_point<point>(end))
      endp = *p2;
    else
      return std::nullopt;

    t.start = startp.x;
    t.end = endp.x;
    make_map(t, startp.y, endp.y, map);
    return t;
  }

  template <avnd::curve T>
  void operator()(T& curve, const std::vector<ossia::value>& segments)
  {
    // Possibilities:
    // {
    //   start:  { x:, y: },
    //   end:    { x:, y: },
    //   map:    "someEasing"    //! TODO:   { function: "power", gamma: 1.3 }
    //   OR gamma:  1.3
    //   OR power:  1.3
    //   OR nothing
    // }
    using curve_type = T;
    using segment_type = typename curve_type::value_type;
    curve.clear();
    for(auto& segment : segments)
    {
      const ossia::value* start{};
      const ossia::value* end{};
      const ossia::value* map{};

      if(auto map_segmt = segment.target<ossia::value_map_type>())
      {
        for(auto& [k, v] : *map_segmt)
        {
          if(k == "start")
            start = &v;
          if(k == "end")
            end = &v;
          if(k == "map" || k == "easing" || k == "function" || k == "gamma"
             || k == "power")
            map = &v;
        }
        if(!start || !end)
          goto error;
      }
      else if(auto array_segmt = segment.target<std::vector<ossia::value>>())
      {
        switch(array_segmt->size())
        {
          case 3:
            map = &((*array_segmt)[2]);
            [[fallthrough]];
          case 2: {
            start = &((*array_segmt)[0]);
            end = &((*array_segmt)[1]);
            break;
          }
          default:
            goto error;
        }
      }

      if(auto s = make_segment<segment_type>(*start, *end, map))
        curve.push_back(std::move(*s));
      else
        goto error;
    }
    return;

  error:
    curve.clear();
    return;
  }
};

}
