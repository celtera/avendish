
#pragma once
#include <cmath>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
struct ArrayRecombiner
{
  halp_meta(name, "Array Recombiner")
  halp_meta(c_name, "avnd_array_recombiner")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(description, "Convert a flat input array into a sequence of arrays")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/array-utilities.html#recombine")
  halp_meta(uuid, "8a833254-04ef-42f0-bd39-a8a3b8ce94c3")
  halp_flag(cv);
  halp_flag(stateless);

  struct
  {
    halp::spinbox_i32<"Grouping", halp::irange{1, 128, 3}> elements;
  } inputs;

  struct do_process_one
  {
    std::vector<ossia::value>& out;
    void operator()(ossia::impulse& v) const noexcept { out.emplace_back(v); }
    void operator()(int& v) const noexcept { out.emplace_back(v); }
    void operator()(float& v) const noexcept { out.emplace_back(v); }
    void operator()(bool& v) const noexcept { out.emplace_back(v); }
    void operator()(std::string& v) const noexcept { out.emplace_back(std::move(v)); }
    void operator()(ossia::vec2f& v) const noexcept
    {
      for(auto& val : v)
        out.emplace_back(val);
    }
    void operator()(ossia::vec3f& v) const noexcept
    {
      for(auto& val : v)
        out.emplace_back(val);
    }
    void operator()(ossia::vec4f& v) const noexcept
    {
      for(auto& val : v)
        out.emplace_back(val);
    }
    void operator()(std::vector<ossia::value>& v) const noexcept
    {
      for(auto& val : v)
        out.emplace_back(std::move(val));
    }
    void operator()(std::vector<std::pair<std::string, ossia::value>>& v) const noexcept
    {
      for(auto& val : v)
        out.emplace_back(std::move(val.second));
    }
    void operator()() const noexcept { out.push_back(ossia::value{}); }
  };

  struct do_process_rec
  {
    std::vector<ossia::value>& out;
    bool recursive{};
    void operator()(ossia::impulse& v) const noexcept { out.emplace_back(v); }
    void operator()(int& v) const noexcept { out.emplace_back(v); }
    void operator()(float& v) const noexcept { out.emplace_back(v); }
    void operator()(bool& v) const noexcept { out.emplace_back(v); }
    void operator()(std::string& v) const noexcept { out.emplace_back(std::move(v)); }
    void operator()(ossia::vec2f& v) const noexcept
    {
      for(auto& val : v)
        out.emplace_back(val);
    }
    void operator()(ossia::vec3f& v) const noexcept
    {
      for(auto& val : v)
        out.emplace_back(val);
    }
    void operator()(ossia::vec4f& v) const noexcept
    {
      for(auto& val : v)
        out.emplace_back(val);
    }
    void operator()(std::vector<ossia::value>& v) const noexcept
    {
      if(recursive)
        for(auto& val : v)
          std::move(val).apply(*this);
      else
        for(auto& val : v)
          std::move(val).apply(do_process_one{out});
    }
    void operator()(std::vector<std::pair<std::string, ossia::value>>& v) const noexcept
    {
      if(recursive)
        for(auto& val : v)
          std::move(val.second).apply(*this);
      else
        for(auto& val : v)
          std::move(val.second).apply(do_process_one{out});
    }
    void operator()() const noexcept { out.push_back(ossia::value{}); }
  };


  std::vector<ossia::value> operator()(ossia::value in)
  {
    std::vector<ossia::value> res;

    if(auto pvec = in.target<std::vector<ossia::value>>())
    {
      auto& vec = *pvec;
      if(inputs.elements <= 0)
        return vec;

      const auto N = vec.size();
      res.reserve(N / inputs.elements);

      for(std::size_t i = 0; i < N; i += inputs.elements)
      {
        std::vector<ossia::value> sub;
        for(std::size_t j = 0; j < (std::size_t)inputs.elements && i + j < N;  j++) {
          sub.push_back(std::move(vec[i + j]));
        }
        res.emplace_back(std::move(sub));
      }
    }

    return res;
  }
};
}
