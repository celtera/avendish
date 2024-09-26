#pragma once

/**
 * \macro generate_port_state_holders
 *
 * This beautiful macro will generate data types that allow to store per-port data,
 * for ports matching a specific predicate, in an optimal way memory-wise.
 *
 * e.g. given for instance an object with:
 *
 * ```
 * struct inputs {
 *   struct { int color; int value; } a;
 *   struct { std::string value; } b;
 *   struct { int color; float** samples; } c;
 * };
 * struct outputs {
 *   struct { float value; } a;
 * };
 * ```
 *
 * and a concept:
 *
 * ```
 * template<typename T>
 * concept has_color = requires(T t) { t.color; };
 * ```
 *
 * Then given the state class:
 *
 * ```
 * struct state_for_color {
 *   std::string hue;
 * };
 * ```
 *
 * The macro call `generate_port_state_holders(has_color, state_for_color)`
 * will create `state_for_color_input_storage` and `state_for_color_output_storage`.
 *
 * In our example, they will look like this: not a single byte is wasted.
 *
 * ```
 * struct state_for_color_input_storage {
 *   // one for A, one for C
 *   tuple<state_for_color, state_for_color> handles;
 * };
 * struct state_for_color_output_storage {
 * };
 * ```
 *
 */
#define generate_port_state_holders(Concept, State)                                 \
  template <typename Field>                                                         \
  struct State##_type;                                                              \
                                                                                    \
  template <Concept Field>                                                          \
  struct State##_type<Field> : State                                                \
  {                                                                                 \
  };                                                                                \
                                                                                    \
  template <typename T>                                                             \
  struct State##_input_storage                                                      \
  {                                                                                 \
  };                                                                                \
                                                                                    \
  template <typename T>                                                             \
    requires(Concept##_inputs_introspection<T>::size > 0)                           \
  struct State##_input_storage<T>                                                   \
  {                                                                                 \
    using hdl_tuple                                                                 \
        = avnd::filter_and_apply<State##_type, Concept##_inputs_introspection, T>;  \
                                                                                    \
    [[no_unique_address]] hdl_tuple handles;                                        \
  };                                                                                \
                                                                                    \
  template <typename T>                                                             \
  struct State##_output_storage                                                     \
  {                                                                                 \
  };                                                                                \
                                                                                    \
  template <typename T>                                                             \
    requires(Concept##_outputs_introspection<T>::size > 0)                          \
  struct State##_output_storage<T>                                                  \
  {                                                                                 \
    using hdl_tuple                                                                 \
        = avnd::filter_and_apply<State##_type, Concept##_outputs_introspection, T>; \
                                                                                    \
    [[no_unique_address]] hdl_tuple handles;                                        \
  }
\
