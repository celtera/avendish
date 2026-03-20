#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/coroutines.hpp>
#include <avnd/common/no_unique_address.hpp>
#include <avnd/concepts/all.hpp>

#include <vector>

namespace avnd
{

template <typename T>
struct member_iterator_one_effect
{
  T & self;
  int i = 0;
  explicit member_iterator_one_effect(T& self) noexcept
      : self{self}
  {
  }

  void operator++() noexcept { i++; }
  auto& operator*() const noexcept { return self.effect; }
  bool operator==(std::default_sentinel_t) const noexcept
  {
    return i > 0;
  }
  member_iterator_one_effect begin() const noexcept {
    return *this;
  }
  auto end() const noexcept {
    return std::default_sentinel;
  }
};

template <typename T>
struct member_iterator_poly_effect
{
  T & self;
  int i = 0;
  explicit member_iterator_poly_effect(T& self) noexcept
      : self{self}
  {
  }

  void operator++() noexcept { i++; }
  auto& operator*() const noexcept
  {
    if constexpr(std::is_same_v<decltype(self.effect), typename T::type>)
      return self.effect[i];
    else
      return self.effect[i].effect;
  }
  bool operator==(std::default_sentinel_t) const noexcept
  {
    return i >= self.effect.size();
  }
  member_iterator_poly_effect begin() const noexcept {
    return *this;
  }
  auto end() const noexcept {
    return std::default_sentinel;
  }
};

template <typename T>
struct full_state_iterator
{
  T& self;
  int i = 0;
  explicit full_state_iterator(T& self) noexcept
      : self{self}
  {
  }

  void operator++() noexcept { i++; }
  auto operator*() const noexcept { return self.full_state(i); }
  bool operator==(std::default_sentinel_t) const noexcept
  {
    return i >= self.effect.size();
  }
  full_state_iterator begin() const noexcept { return *this; }
  auto end() const noexcept { return std::default_sentinel; }
};

template <typename T>
struct inputs_storage
{
};

template <inputs_is_type T>
struct inputs_storage<T>
{
  typename T::inputs inputs_storage;
};

template <typename T>
struct outputs_storage
{
};

template <outputs_is_type T>
struct outputs_storage<T>
{
  typename T::outputs outputs_storage;
};

/**
 * @brief used to adapt monophonic effects to polyphonic hosts
 */
template <typename T>
struct effect_container
    : inputs_storage<T>
    , outputs_storage<T>
{
  using type = T;
  enum
  {
    single_instance
  };

  T effect;

  inline constexpr void init_channels(int input, int output)
  {
    // TODO maybe a runtime check
  }

  inline constexpr auto& inputs() noexcept
  {
    if constexpr(has_inputs<T>)
    {
      if constexpr(inputs_is_type<T>)
        return this->inputs_storage;
      else
        return effect.inputs;
    }
    else
      return dummy_instance;
  }

  inline constexpr auto& inputs() const noexcept
  {
    if constexpr(has_inputs<T>)
    {
      if constexpr(inputs_is_type<T>)
        return this->inputs_storage;
      else
        return effect.inputs;
    }
    else
      return dummy_instance;
  }

  inline constexpr auto& outputs() noexcept
  {
    if constexpr(has_outputs<T>)
    {
      if constexpr(outputs_is_type<T>)
        return this->outputs_storage;
      else
        return effect.outputs;
    }
    else
      return dummy_instance;
  }

  inline constexpr auto& outputs() const noexcept
  {
    if constexpr(has_outputs<T>)
    {
      if constexpr(outputs_is_type<T>)
        return this->outputs_storage;
      else
        return effect.outputs;
    }
    else
      return dummy_instance;
  }

  struct ref
  {
    T& effect;
    decltype(std::declval<effect_container>().inputs())& inputs;
    decltype(std::declval<effect_container>().outputs())& outputs;
  };

  inline std::array<ref, 1> full_state() noexcept
  {
    return {ref{effect, this->inputs(), this->outputs()}};
  }

  inline auto effects() noexcept
  {
    return member_iterator_one_effect<effect_container>{*this};
  }
};

template <typename T>
  requires(
      !has_inputs<T> && !has_outputs<T> && !monophonic_audio_processor<T> && !tag_cv<T>)
struct effect_container<T>
{
  using type = T;
  enum
  {
    single_instance
  };

  T effect;

  void init_channels(int input, int output)
  {
    // TODO maybe a runtime check
  }

  auto& inputs() noexcept { return dummy_instance; }
  auto& inputs() const noexcept { return dummy_instance; }
  auto& outputs() noexcept { return dummy_instance; }
  auto& outputs() const noexcept { return dummy_instance; }

  auto effects() { return member_iterator_one_effect<effect_container>{*this}; }

  struct ref
  {
    T& effect;

    AVND_NO_UNIQUE_ADDRESS dummy inputs;

    AVND_NO_UNIQUE_ADDRESS dummy outputs;
  };

  std::array<ref, 1> full_state() noexcept { return {ref{effect, {}, {}}}; }
};

template <typename T>
  requires(
      !has_inputs<T> && !has_outputs<T> && monophonic_audio_processor<T> && !tag_cv<T>)
struct effect_container<T>
{
  using type = T;
  enum
  {
    multi_instance
  };

  std::vector<T> effect;
  void init_channels(int input, int output)
  {
    // FIXME do that everywhere
    // FIXME how to save controls when we go down to 0 channels ?
    if(effect.empty())
      effect.resize(input);
    else if(effect.size() > input)
      effect.resize(input);
    else
      while(effect.size() < input)
        effect.push_back(effect[0]);
  }

  auto& inputs() noexcept { return dummy_instance; }
  auto& inputs() const noexcept { return dummy_instance; }
  auto& outputs() noexcept { return dummy_instance; }
  auto& outputs() const noexcept { return dummy_instance; }

  auto effects()
  {
    return member_iterator_poly_effect<effect_container>{*this};
  }

  struct ref
  {
    T& effect;

    AVND_NO_UNIQUE_ADDRESS dummy inputs;

    AVND_NO_UNIQUE_ADDRESS dummy outputs;
  };

  member_iterator<ref> full_state()
  {
    for(auto& e : effect)
    {
      ref r{e, {}, {}};
      co_yield r;
    }
  }
};

template <avnd::monophonic_audio_processor T>
  requires(avnd::inputs_is_type<T> && !avnd::has_outputs<T> && !tag_cv<T>)
struct effect_container<T>
{
  using type = T;
  enum
  {
    multi_instance
  };

  typename T::inputs inputs_storage;

  std::vector<T> effect;

  void init_channels(int input, int output) { effect.resize(std::max(input, output)); }

  auto& inputs() noexcept { return inputs_storage; }
  auto& inputs() const noexcept { return inputs_storage; }
  auto& outputs() noexcept { return dummy_instance; }
  auto& outputs() const noexcept { return dummy_instance; }

  struct ref
  {
    T& effect;
    typename T::inputs& inputs;

    AVND_NO_UNIQUE_ADDRESS avnd::dummy outputs;
  };

  ref full_state(int i) { return {effect[i], this->inputs_storage, dummy_instance}; }
  full_state_iterator<effect_container> full_state()
  {
    return full_state_iterator<effect_container>{*this};
  }

  auto effects()
  {
    return member_iterator_poly_effect<effect_container>{*this};
  }
};

template <avnd::monophonic_audio_processor T>
  requires(avnd::inputs_is_type<T> && avnd::outputs_is_type<T> && !tag_cv<T>)
struct effect_container<T>
{
  using type = T;
  enum
  {
    multi_instance
  };

  typename T::inputs inputs_storage;

  struct state
  {
    T effect;
    typename T::outputs outputs_storage;
  };

  std::vector<state> effect;

  void init_channels(int input, int output) { effect.resize(std::max(input, output)); }

  auto& inputs() noexcept { return inputs_storage; }
  auto& inputs() const noexcept { return inputs_storage; }

  struct ref
  {
    T& effect;
    typename T::inputs& inputs;
    typename T::outputs& outputs;
  };

  ref full_state(int i)
  {
    return {effect[i].effect, this->inputs_storage, effect[i].outputs_storage};
  }

  full_state_iterator<effect_container> full_state()
  {
    return full_state_iterator<effect_container>{*this};
  }

  auto effects()
  {
    return member_iterator_poly_effect<effect_container>{*this};
  }

  member_iterator<typename T::outputs> outputs()
  {
    for(auto& e : effect)
      co_yield e.outputs_storage;
  }
};

template <avnd::monophonic_audio_processor T>
  requires(avnd::inputs_is_type<T> && avnd::outputs_is_value<T> && !tag_cv<T>)
struct effect_container<T>
{
  using type = T;
  enum
  {
    multi_instance
  };

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

  ref full_state(int i)
  {
    return {effect[i].effect, this->inputs_storage, effect[i].effect.outputs};
  }

  full_state_iterator<effect_container> full_state()
  {
    return full_state_iterator<effect_container>{*this};
  }

  auto effects()
  {
    return member_iterator_poly_effect<effect_container>{*this};
  }

  member_iterator<typename T::outputs> outputs()
  {
    for(auto& e : effect)
      co_yield e.output;
  }
};

template <avnd::monophonic_audio_processor T>
  requires(avnd::inputs_is_value<T> && avnd::outputs_is_value<T> && !tag_cv<T>)
struct effect_container<T>
{
  using type = T;
  enum
  {
    multi_instance
  };

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

  ref full_state(int i) { return {effect[i], effect[i].inputs, effect[i].outputs}; }

  full_state_iterator<effect_container> full_state()
  {
    return full_state_iterator<effect_container>{*this};
  }

  auto effects() { return member_iterator_poly_effect<effect_container>{*this}; }

  member_iterator<decltype(T::inputs)> inputs()
  {
    for(auto& e : effect)
      co_yield e.inputs;
  }
  member_iterator<decltype(T::outputs)> outputs()
  {
    for(auto& e : effect)
      co_yield e.outputs;
  }
};
template <avnd::monophonic_audio_processor T>
  requires(avnd::inputs_is_value<T> && !avnd::has_outputs<T> && !tag_cv<T>)
struct effect_container<T>
{
  using type = T;
  enum
  {
    multi_instance
  };

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

    AVND_NO_UNIQUE_ADDRESS avnd::dummy outputs;
  };

  ref full_state(int i)
  {
    return {effect[i].effect, effect[i].inputs, avnd::dummy_instance};
  }

  full_state_iterator<effect_container> full_state()
  {
    return full_state_iterator<effect_container>{*this};
  }

  auto effects()
  {
    return member_iterator_poly_effect<effect_container>{*this};
  }

  member_iterator<decltype(T::inputs)> inputs()
  {
    for(auto& e : effect)
      co_yield e.inputs;
  }

  auto& outputs() noexcept { return dummy_instance; }
  auto& outputs() const noexcept { return dummy_instance; }
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
