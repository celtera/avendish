#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/concepts.hpp>
#include <avnd/common/coroutines.hpp>

#include <vector>

namespace avnd
{
/**
 * @brief used to adapt monophonic effects to polyphonic hosts
 */
template <typename T>
struct effect_container
{
  using type = T;

  T effect;

  void init_channels(int input, int output)
  {
    // TODO maybe a runtime check
  }

  auto& inputs() noexcept { return effect.inputs; }
  auto& inputs() const noexcept { return effect.inputs; }
  auto& outputs() noexcept { return effect.outputs; }
  auto& outputs() const noexcept { return effect.outputs; }

  member_iterator<T> effects() { co_yield effect; }
};

template <typename T>
requires (!has_inputs<T> && !has_outputs<T>)
struct effect_container<T>
{
  using type = T;

  T effect;

  void init_channels(int input, int output)
  {
    // TODO maybe a runtime check
  }

  auto& inputs() noexcept { return dummy_instance; }
  auto& inputs() const noexcept { return dummy_instance; }
  auto& outputs() noexcept { return dummy_instance; }
  auto& outputs() const noexcept { return dummy_instance; }

  member_iterator<T> effects() { co_yield effect; }

  struct ref
  {
    T& effect;

    [[no_unique_address]]
    dummy inputs;

    [[no_unique_address]]
    dummy outputs;
  };

  member_iterator<ref> full_state()
  {
    ref r{effect, {}, {}};
    co_yield r;
  }
};

template <avnd::monophonic_audio_processor T>
requires avnd::inputs_is_type<T> && avnd::outputs_is_type<T>
struct effect_container<T>
{
  using type = T;

  typename T::inputs inputs_storage;

  struct state
  {
    T effect;
    typename T::outputs outputs_storage;
  };

  std::vector<state> effect;

  void init_channels(int input, int output)
  {
    effect.resize(std::max(input, output));
  }

  auto& inputs() noexcept { return inputs_storage; }
  auto& inputs() const noexcept { return inputs_storage; }

  struct ref
  {
    T& effect;
    typename T::inputs& inputs;
    typename T::outputs& outputs;
  };

  member_iterator<ref> full_state()
  {
    for (state& e : effect)
    {
      ref r{e.effect, this->inputs_storage, e.outputs_storage};
      co_yield r;
    }
  }

  member_iterator<T> effects()
  {
    for (auto& e : effect)
      co_yield e.effect;
  }

  member_iterator<typename T::outputs> outputs()
  {
    for (auto& e : effect)
      co_yield e.outputs_storage;
  }
};

template <avnd::monophonic_audio_processor T>
requires avnd::inputs_is_type<T> && avnd::outputs_is_value<T>
struct effect_container<T>
{
  using type = T;

  typename T::inputs inputs_storage;

  std::vector<T> effect;

  void init_channels(int input, int output)
  {
    assert(input == output);
    effect.resize(input);
  }

  auto& inputs() noexcept { return inputs_storage; }
  auto& inputs() const noexcept { return inputs_storage; }

  struct ref
  {
    T& effect;
    typename T::inputs& inputs;
    decltype(T::outputs)& outputs;
  };
  member_iterator<ref> full_state()
  {
    for (T& e : effect)
    {
      ref r{e.effect, this->inputs_storage, e.effect.outputs};
      co_yield r;
    }
  }

  member_iterator<T> effects()
  {
    for (auto& e : effect)
      co_yield e;
  }

  member_iterator<typename T::outputs> outputs()
  {
    for (auto& e : effect)
      co_yield e.output;
  }
};

template <avnd::monophonic_audio_processor T>
requires avnd::inputs_is_value<T> && avnd::outputs_is_value<T>
struct effect_container<T>
{
  using type = T;

  std::vector<T> effect;

  void init_channels(int input, int output)
  {
    // TODO maybe a runtime check
    int orig = effect.size();
    int sz = std::max(input, output);
    effect.resize(sz);
    if(orig > 0)
    {
      for(int i = orig; i < sz; i++)
      {
        effect[i].inputs = effect[0].inputs;
      }
    }
  }

  struct ref
  {
    T& effect;
    decltype(T::inputs)& inputs;
    decltype(T::outputs)& outputs;
  };
  member_iterator<ref> full_state()
  {
    for (T& e : effect)
    {
      ref r{e.effect, e.effect.inputs, e.effect.outputs};
      co_yield r;
    }
  }

  member_iterator<T> effects()
  {
    for (auto& e : effect)
      co_yield e;
  }

  member_iterator<decltype(T::inputs)> inputs()
  {
    for (auto& e : effect)
      co_yield e.input;
  }
  member_iterator<decltype(T::outputs)> outputs()
  {
    for (auto& e : effect)
      co_yield e.output;
  }
};

template <typename T>
struct get_object_type
{
  using type = T;
};
template <typename T>
struct get_object_type<effect_container<T>>
{
  using type = T;
};

template <typename T>
using get_object_type_t = typename get_object_type<T>::type;

}
