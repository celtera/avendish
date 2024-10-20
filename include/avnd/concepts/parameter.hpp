#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/mapper.hpp>
#include <avnd/concepts/range.hpp>
#include <avnd/concepts/smooth.hpp>
#include <avnd/concepts/widget.hpp>

#include <string>

namespace avnd
{
/**
 * A "parameter" port is something that has a value:
 *
 * struct {
 *   float value;
 * };
 */

#if defined(_MSC_VER)

#define AVND_REQUIREMENT_ON_MEMBER(Field, Member, Requirement)   \
  (                                                              \
      requires { Field::Member().Requirement; }                  \
      || requires { std::declval<Field::Member>().Requirement; } \
      || requires { Field::Member.Requirement; })

#define AVND_CONCEPT_CHECK_ON_MEMBER(Concept, Field, Member, Requirement) \
  (Concept<decltype(Field::Member().Requirement)>                         \
   || Concept<decltype(Field::Member.Requirement)>                        \
   || Concept<decltype(std::declval<Field::Member>().Requirement)>)
#else

#define AVND_REQUIREMENT_ON_MEMBER(Field, Member, Requirement) \
  (                                                            \
      requires { Field::Member().Requirement; }                \
      || requires { (typename Field::Member){}.Requirement; }  \
      || requires { Field::Member.Requirement; })

#define AVND_CONCEPT_CHECK_ON_MEMBER(Concept, Field, Member, Requirement) \
  (Concept<decltype(Field::Member().Requirement)>                         \
   || Concept<decltype(Field::Member.Requirement)>                        \
   || Concept<decltype((typename Field::Member){}.Requirement)>)
#endif

// Concepts related to inputs / outputs
template <typename T>
concept parameter = std::is_default_constructible_v<decltype(T::value)>;

template <typename T>
concept int_parameter = requires(T t) {
                          {
                            t.value
                            } -> int_ish;
                        };

template <typename T>
concept enum_parameter = std::is_enum_v<std::decay_t<decltype(T::value)>>;

template <typename Field>
concept enum_ish_parameter
    = avnd::enum_parameter<Field>
      || (avnd::has_range<Field> && AVND_REQUIREMENT_ON_MEMBER(Field, range, values[0]));

template <typename T>
concept float_parameter = requires(T t) {
                            {
                              t.value
                              } -> fp_ish;
                          };

template <typename T>
concept bool_parameter = requires(T t) {
                           {
                             t.value
                             } -> bool_ish;
                         };

template <typename T>
concept string_parameter = requires(T t) {
                             {
                               t.value
                               } -> std::convertible_to<std::string>;
                           };

template <typename T>
concept xy_value = requires(T t) {
                     t.x;
                     t.y;
                   };
template <typename T>
concept xy_parameter = xy_value<decltype(T::value)>;

template <typename T>
concept rgb_value = requires(T t) {
                      t.r;
                      t.g;
                      t.b;
                    };
template <typename T>
concept rgb_parameter = rgb_value<decltype(T::value)>;

template <typename C>
concept parameter_with_minmax_range = AVND_REQUIREMENT_ON_MEMBER(C, range, min)
                                      && AVND_REQUIREMENT_ON_MEMBER(C, range, max)
                                      && AVND_REQUIREMENT_ON_MEMBER(C, range, init);

template <typename C>
concept integer_or_enum
    = std::integral<std::decay_t<C>> || std::is_enum_v<std::decay_t<C>>;

template <typename C>
concept parameter_with_values_range
    = AVND_REQUIREMENT_ON_MEMBER(C, range, values[0])
      && AVND_CONCEPT_CHECK_ON_MEMBER(integer_or_enum, C, range, init);

// Used for defining process which take programs in some lang as input
template <typename T>
concept program_parameter
    = string_parameter<T> && requires {
                               {
                                 T::language()
                                 } -> std::convertible_to<std::string>;
                             };

/**
 * Here T is a e.g. std::span<T>
 */
template <typename T>
concept span_value
    = requires(T t) {
        begin(t);
        end(t);
        t.extent;
        t.data();
        t.size();
      } && std::is_constructible_v<T, typename T::value_type*, std::size_t> && std::is_trivially_copy_constructible_v<T>;
template <typename T>
concept span_parameter = parameter<T> && span_value<std::decay_t<decltype(T::value)>>;

/**
 * Timed values are used for sample-accurate ports.
 * That is, ports where the time (sample) at which the value
 * happened is known.
 */

/**
 * Here T is for instance `std::optional<float>* values`
 * where values[5] = 123.f; means that the value 123
 * happened at sample 5 since the beginning of the audio buffer.
 */
template <typename T>
concept linear_timed_values = std::is_pointer_v<T> && requires(T t) { *(t[0]); };

/**
 * Here T is a e.g. std::span<timed_value>
 * where
 *
 * struct timed_value {
 *   int frame;
 *   float value;
 * };
 */
template <typename T>
concept span_timed_values = span_value<T> && requires(T t) {
                                               {
                                                 T::value_type::frame
                                                 } -> avnd::int_ish;
                                               T::value_type::value;
                                             };

/**
 * Here T is a e.g. std::map<int, float> and manages its own allocations
 */
template <typename T>
concept dynamic_timed_values = requires(T t) { typename T::allocator_type; };

/**
 * A sample-accurate parameter has an additional "values" member
 * which allows to know at which sample did a control change
 */
template <typename T>
concept sample_accurate_parameter
    = parameter<T>
      && (
          requires(T t) { t.value = *t.values[1]; }               // linear map
          || requires(T t) { t.value = t.values[1]; }             // dynamic map
          || requires(T t) { t.value = t.values.begin()->value; } // span
      );

template <typename T>
concept linear_sample_accurate_parameter
    = sample_accurate_parameter<T>
      && linear_timed_values<std::decay_t<decltype(T::values)>>;
template <typename T>
concept span_sample_accurate_parameter
    = sample_accurate_parameter<T>
      && span_timed_values<std::decay_t<decltype(T::values)>>;
template <typename T>
concept dynamic_sample_accurate_parameter
    = sample_accurate_parameter<T>
      && dynamic_timed_values<std::decay_t<decltype(T::values)>>;

/**
 * Parameters that want to be smoothed (range is needed)
 */
template <typename T>
concept smooth_parameter = parameter<T> && has_range<T> && has_smooth<T>;

}
