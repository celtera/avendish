#pragma once
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
// #include <boost/smart_ptr/atomic_shared_ptr.hpp>
#include <ossia/detail/lockfree_queue.hpp>

#include <bitset>

namespace oscr
{

template <typename Field>
struct controls_type;

template <avnd::parameter Field>
  requires avnd::control<Field>
struct controls_type<Field>
{
  using type = std::decay_t<decltype(Field::value)>;
};

template <typename Field>
using controls_type_t = typename controls_type<Field>::type;

template <avnd::callback Field>
  requires avnd::control<Field>
struct controls_type<Field>
{
  // FIXME maybe the tuple of arguments?
  using type = ossia::value;
};

#if 0
template <typename T>
using atomic_shared_ptr = boost::atomic_shared_ptr<T>;

template <typename T>
struct controls_mirror
{
  static constexpr int i_size = avnd::control_input_introspection<T>::size;
  static constexpr int o_size = avnd::control_output_introspection<T>::size;
  using i_tuple
      = avnd::filter_and_apply<controls_type, avnd::control_input_introspection, T>;
  using o_tuple
      = avnd::filter_and_apply<controls_type, avnd::control_output_introspection, T>;

  controls_mirror()
  {
    inputs.load(new i_tuple);
    outputs.load(new o_tuple);
  }

  AVND_NO_UNIQUE_ADDRESS atomic_shared_ptr<i_tuple> inputs;
  AVND_NO_UNIQUE_ADDRESS atomic_shared_ptr<o_tuple> outputs;

  std::bitset<i_size> inputs_bits;
  std::bitset<o_size> outputs_bits;
};
#endif

template <typename T>
struct controls_input_queue
{
  using i_tuple = std::tuple<>;
};
template <typename T>
struct controls_output_queue
{
  using o_tuple = std::tuple<>;
};
template <typename T>
  requires(avnd::control_input_introspection<T>::size > 0)
struct controls_input_queue<T>
{
  static constexpr int i_size = avnd::control_input_introspection<T>::size;
  using i_tuple
      = avnd::filter_and_apply<controls_type_t, avnd::control_input_introspection, T>;

  ossia::mpmc_queue<i_tuple> ins_queue;
  std::bitset<i_size> inputs_set;
};

template <typename T>
  requires(avnd::control_output_introspection<T>::size > 0)
struct controls_output_queue<T>
{
  static constexpr int o_size = avnd::control_output_introspection<T>::size;
  using o_tuple
      = avnd::filter_and_apply<controls_type_t, avnd::control_output_introspection, T>;

  ossia::mpmc_queue<o_tuple> outs_queue;

  std::bitset<o_size> outputs_set;
};

template <typename T>
struct controls_queue
    : controls_input_queue<T>
    , controls_output_queue<T>
{
};

}
