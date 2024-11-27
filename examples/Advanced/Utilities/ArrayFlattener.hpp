
#pragma once
#include <cmath>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
struct ArrayFlattener
{
  halp_meta(name, "Array Flattener")
  halp_meta(c_name, "avnd_array_flattener")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(description, "Flatten an input array")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/array-utilities.html#flatten")
  halp_meta(uuid, "d2cd609b-0032-45f0-a210-de6c2f513012")
  halp_flag(cv);
  halp_flag(stateless);

  struct
  {
    halp::toggle<"Recursive"> recursive;
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
    in.apply(do_process_rec{res, inputs.recursive});
    return res;
  }
};
}
