#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/input.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <boost/container/small_vector.hpp>

#include <variant>

namespace halp
{
template <typename T, typename Attr>
bool parse_attribute(T& object, Attr& attr, auto& arg_begin, auto& arg_end)
{
  using value_type = std::remove_cvref_t<std::decay_t<decltype(*arg_begin)>>;
  boost::container::small_vector<value_type, 64> current_arguments;

  using attr_value_type = std::remove_cvref_t<std::decay_t<decltype(attr.value)>>;
  if(arg_begin == arg_end)
  {
    // we're done.
    if(std::is_same_v<attr_value_type, bool>)
    {
      attr.value = true;
      if constexpr(requires { attr.update(object); })
        attr.update(object);
      return true;
    }
  }

  // FIXME for now we only handle attributes with one argument... handle lists, pairs, tuples etc
  struct subvis_t
  {
    T& object;
    Attr& attr;
    bool finished{};
    bool operator()(std::string_view v) noexcept
    {
      if(v.starts_with("@"))
      {
        // We're done

        finished = true;
        return true;
      }
      else
      {
        if constexpr(requires { attr.value = std::string{}; })
        {
          attr.value = std::string(v);
          if constexpr(requires { attr.update(object); })
            attr.update(object);
          return true;
        }
      }
      return false;
    }

    bool operator()(double v) const noexcept
    {
      if constexpr(requires { attr.value = v; })
      {
        attr.value = v;
        if constexpr(requires { attr.update(object); })
          attr.update(object);
        return true;
      }
      return false;
    }
  } vis{object, attr};

  for(auto& it = arg_begin; it != arg_end; ++it)
  {
    if(!std::visit(vis, *it))
    {
      return false;
    }
    else if(vis.finished)
    {
      return true;
    }
  }
  return true;
}

static constexpr bool
string_matches_attribute_name(std::string_view string_a, std::string_view string_b)
{
  const int N = string_a.length();
  if(N != string_b.length())
    return false;

  for(int i = 0; i < N; i++)
  {
    unsigned char a = string_a[i];
    unsigned char b = string_b[i];

    if(a >= 'a' && a <= 'z')
      a -= 'a' - 'A';
    if(b >= 'a' && b <= 'z')
      b -= 'a' - 'A';
    if(a >= '0' && a <= '9' && a >= 'A' && a <= 'Z')
    {
      if(b >= '0' && b <= '9' && b >= 'A' && b <= 'Z')
      {
        if(a == b)
          continue;
        else
          return false;
      }
      else
      {
        return false;
      }
    }
    else
    {
      // Let's assume all special chars compare equal for now
      if(!(b >= '0' && b <= '9' && b >= 'A' && b <= 'Z'))
        continue;
    }
  }
  return true;
}

template <typename T>
void parse_attributes(T& object, auto& init_arguments)
{
  using iterator_t = std::remove_cvref_t<decltype(init_arguments.begin())>;

  struct vis_t
  {
    T& object;
    iterator_t begin, end;
    bool operator()(std::string_view v) noexcept
    {
      if(v.starts_with("@"))
      {
        // Locate argument
        v = v.substr(1);
        return avnd::input_introspection<T>::for_all_until(
            object.inputs, [&]<typename Input>(Input& in) {
              static constexpr auto nm = avnd::get_name<Input>();
              if(string_matches_attribute_name(v, nm))
              {
                return parse_attribute(object, in, begin, end);
              }
              return false;
            });
      }
      return false;
    }

    bool operator()(double v) const noexcept { return false; }

  } vis{object, init_arguments.begin(), init_arguments.end()};


  for(;;)
  {
    if(vis.begin == vis.end)
      return;
    if(!std::visit(vis, *vis.begin++))
    {
      break;
    }
  }
}
}
