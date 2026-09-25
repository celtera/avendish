#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
// Appended to, never reordered: a saved score stores the mode.
enum class ArrayCombinerMode
{
  // Element-wise: one array out, as long as the longest input.
  Sum,
  Append,
  Product,
  Intersperse,
  Min,
  Max,
  Mean,
  Subtract,
  AbsDifference,
  Divide,
  Median,
  Clamp,
  // Input 1 against each other input: one number per other input.
  CosineSimilarity,
  DotProduct,
  EuclideanDistance,
  ManhattanDistance,
  // Element-wise, chained (a > b > c): 1 where it holds, else 0.
  Greater,
  Less,
  GreaterEqual,
  LessEqual,
};

struct ArrayCombiner
{
  halp_meta(name, "Array Value Combiner")
  halp_meta(c_name, "avnd_array_combiner")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      description,
      "Combine input arrays: element-wise (sum, mean, difference, median, "
      "comparisons, ...), or compare the first input with each other one "
      "(cosine similarity, dot product, distances).")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/array-utilities.html#sum")
  halp_meta(uuid, "f5b8e20e-a016-4e19-a396-39d780179533")

  struct
  {
    struct : halp::spinbox_i32<"Input count", halp::range{0, 1024, 1}>
    {
      static std::function<void(ArrayCombiner&, int)> on_controller_interaction()
      {
        return [](ArrayCombiner& object, int value) {
          object.inputs.in_i.request_port_resize(value);
        };
      }
    } controller;

    struct : halp::enum_t<ao::ArrayCombinerMode, "Mode">
    {
      halp_meta(
          description,
          "Sum ... Clamp: element by element; missing elements count as 0 (Mean "
          "and Median use the inputs that have the element; Clamp bounds input 1 "
          "by input 2 below and input 3 above). Cosine similarity ... Manhattan "
          "distance: input 1 against each other input, over their common "
          "length. Greater ... Less or equal: 1 where input 1 > input 2 > ... "
          "holds, else 0.")
    } mode;
    halp::dynamic_port<halp::val_port<"Input {}", std::vector<float>>> in_i;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::vector<float>> out;
  } outputs;

  std::vector<float> m_scratch;

  static bool comparesToFirst(ArrayCombinerMode m) noexcept
  {
    using enum ArrayCombinerMode;
    return m == CosineSimilarity || m == DotProduct || m == EuclideanDistance
           || m == ManhattanDistance;
  }

  std::size_t longest() const noexcept
  {
    std::size_t n = 0;
    for(auto& v : inputs.in_i.ports)
      n = std::max(n, v.value.size());
    return n;
  }

  // Input 1 against input k, over their common length.
  static float pairwise(
      ArrayCombinerMode mode, const std::vector<float>& a,
      const std::vector<float>& b) noexcept
  {
    const std::size_t n = std::min(a.size(), b.size());
    double acc = 0., na = 0., nb = 0.;
    switch(mode)
    {
      case ArrayCombinerMode::CosineSimilarity:
        for(std::size_t i = 0; i < n; i++)
        {
          acc += (double)a[i] * b[i];
          na += (double)a[i] * a[i];
          nb += (double)b[i] * b[i];
        }
        return (na > 0. && nb > 0.) ? float(acc / std::sqrt(na * nb)) : 0.f;
      case ArrayCombinerMode::DotProduct:
        for(std::size_t i = 0; i < n; i++)
          acc += (double)a[i] * b[i];
        return float(acc);
      case ArrayCombinerMode::EuclideanDistance:
        for(std::size_t i = 0; i < n; i++)
        {
          const double d = (double)a[i] - b[i];
          acc += d * d;
        }
        return float(std::sqrt(acc));
      case ArrayCombinerMode::ManhattanDistance:
        for(std::size_t i = 0; i < n; i++)
          acc += std::abs((double)a[i] - b[i]);
        return float(acc);
      default:
        return 0.f;
    }
  }

  static bool compare(ArrayCombinerMode mode, float a, float b) noexcept
  {
    switch(mode)
    {
      case ArrayCombinerMode::Greater:
        return a > b;
      case ArrayCombinerMode::Less:
        return a < b;
      case ArrayCombinerMode::GreaterEqual:
        return a >= b;
      case ArrayCombinerMode::LessEqual:
        return a <= b;
      default:
        return false;
    }
  }

  void operator()()
  {
    const auto num_ports = inputs.in_i.ports.size();
    if(num_ports == 0)
      return;

    auto& out = outputs.out.value;
    const auto mode = inputs.mode.value;
    if(num_ports == 1)
    {
      // Nothing to compare the first input with.
      if(comparesToFirst(mode))
        out.clear();
      else
        out = inputs.in_i.ports[0].value;
      return;
    }

    out.clear();
    auto& ports = inputs.in_i.ports;
    switch(mode)
    {
      case ao::ArrayCombinerMode::Sum: {
        for(auto& v : ports)
        {
          if(v.value.size() > out.size())
          {
            out.resize(v.value.size());
          }
          for(std::size_t i = 0; i < v.value.size(); i++)
          {
            out[i] += v.value[i];
          }
        }

        break;
      }
      case ao::ArrayCombinerMode::Product: {
        for(auto& v : ports)
        {
          if(v.value.size() > out.size())
          {
            out.resize(v.value.size(), 1.f);
          }
          for(std::size_t i = 0; i < v.value.size(); i++)
          {
            out[i] *= v.value[i];
          }
        }

        break;
      }
      case ao::ArrayCombinerMode::Append: {
        std::size_t total_n = 0;
        for(auto& v : ports)
          total_n += v.value.size();
        out.reserve(total_n);

        for(auto& v : ports)
          out.insert(out.end(), v.value.begin(), v.value.end());

        break;
      }
      case ao::ArrayCombinerMode::Intersperse: {
        const std::size_t max_n = longest();
        const auto n = ports.size();
        out.resize(max_n * n);

        for(std::size_t p = 0; p < n; p++)
        {
          auto& in = ports[p].value;
          for(std::size_t i = 0; i < max_n; i++)
          {
            if(in.size() > i)
              out[i * n + p] = in[i];
          }
        }
        break;
      }

      case ao::ArrayCombinerMode::Min:
      case ao::ArrayCombinerMode::Max: {
        const bool is_min = mode == ao::ArrayCombinerMode::Min;
        // 1. Start from the longest array
        std::size_t longest_v = 0;
        std::size_t longest_i = 0;
        for(std::size_t port_i = 0; port_i < num_ports; port_i++)
        {
          const auto sz = ports[port_i].value.size();
          if(longest_v < sz)
          {
            longest_v = sz;
            longest_i = port_i;
          }
        }

        out = ports[longest_i].value;

        // 2. Min / max with the other arrays
        for(std::size_t port_i = 0; port_i < num_ports; port_i++)
        {
          if(port_i == longest_i)
            continue;

          auto& v = ports[port_i];
          for(std::size_t i = 0; i < v.value.size(); i++)
          {
            out[i] = is_min ? std::min(out[i], v.value[i]) : std::max(out[i], v.value[i]);
          }
        }
        break;
      }

      case ao::ArrayCombinerMode::Mean: {
        // Each element is the mean of the inputs long enough to have it.
        const std::size_t n = longest();
        out.assign(n, 0.f);
        for(std::size_t i = 0; i < n; i++)
        {
          int count = 0;
          for(auto& v : ports)
            if(i < v.value.size())
            {
              out[i] += v.value[i];
              count++;
            }
          out[i] /= (float)count;
        }
        break;
      }

      case ao::ArrayCombinerMode::Subtract:
      case ao::ArrayCombinerMode::AbsDifference: {
        out.assign(longest(), 0.f);
        std::copy(ports[0].value.begin(), ports[0].value.end(), out.begin());
        for(std::size_t p = 1; p < num_ports; p++)
        {
          auto& v = ports[p].value;
          for(std::size_t i = 0; i < v.size(); i++)
            out[i] -= v[i];
        }
        if(mode == ao::ArrayCombinerMode::AbsDifference)
          for(auto& x : out)
            x = std::abs(x);
        break;
      }

      case ao::ArrayCombinerMode::Divide: {
        // Input 1 divided by each other input; a division by 0 gives 0.
        out.assign(longest(), 0.f);
        std::copy(ports[0].value.begin(), ports[0].value.end(), out.begin());
        for(std::size_t p = 1; p < num_ports; p++)
        {
          auto& v = ports[p].value;
          for(std::size_t i = 0; i < out.size(); i++)
          {
            const float d = i < v.size() ? v[i] : 0.f;
            out[i] = d != 0.f ? out[i] / d : 0.f;
          }
        }
        break;
      }

      case ao::ArrayCombinerMode::Median: {
        // Per element, over the inputs long enough to have it.
        const std::size_t n = longest();
        out.assign(n, 0.f);
        for(std::size_t i = 0; i < n; i++)
        {
          m_scratch.clear();
          for(auto& v : ports)
            if(i < v.value.size())
              m_scratch.push_back(v.value[i]);
          const std::size_t k = m_scratch.size();
          std::sort(m_scratch.begin(), m_scratch.end());
          out[i] = (k % 2) ? m_scratch[k / 2]
                           : 0.5f * (m_scratch[k / 2 - 1] + m_scratch[k / 2]);
        }
        break;
      }

      case ao::ArrayCombinerMode::Clamp: {
        // Input 1, above input 2 and below input 3 where they have the
        // element. Further inputs are ignored.
        out = ports[0].value;
        auto& lo = ports[1].value;
        const std::vector<float>* hi = num_ports > 2 ? &ports[2].value : nullptr;
        for(std::size_t i = 0; i < out.size(); i++)
        {
          if(i < lo.size())
            out[i] = std::max(out[i], lo[i]);
          if(hi && i < hi->size())
            out[i] = std::min(out[i], (*hi)[i]);
        }
        break;
      }

      case ao::ArrayCombinerMode::CosineSimilarity:
      case ao::ArrayCombinerMode::DotProduct:
      case ao::ArrayCombinerMode::EuclideanDistance:
      case ao::ArrayCombinerMode::ManhattanDistance: {
        out.resize(num_ports - 1);
        for(std::size_t p = 1; p < num_ports; p++)
          out[p - 1] = pairwise(mode, ports[0].value, ports[p].value);
        break;
      }

      case ao::ArrayCombinerMode::Greater:
      case ao::ArrayCombinerMode::Less:
      case ao::ArrayCombinerMode::GreaterEqual:
      case ao::ArrayCombinerMode::LessEqual: {
        // a > b > c: every consecutive pair holds. A missing element fails.
        const std::size_t n = longest();
        out.assign(n, 1.f);
        for(std::size_t i = 0; i < n; i++)
        {
          for(std::size_t p = 0; p + 1 < num_ports; p++)
          {
            auto& a = ports[p].value;
            auto& b = ports[p + 1].value;
            if(i >= a.size() || i >= b.size() || !compare(mode, a[i], b[i]))
            {
              out[i] = 0.f;
              break;
            }
          }
        }
        break;
      }
    }
  }
};
}
