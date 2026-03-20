#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/controls_double.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <CPlusPlus_Common.h>

#include <charconv>
#include <iterator>
#include <string>
#include <type_traits>

namespace touchdesigner
{

/**
 * Utility to convert a value to a string for Info DAT cells.
 * Handles all common C++ value types.
 */
inline void value_to_string(char* buf, std::size_t buf_size, float v)
{
  auto [ptr, ec] = std::to_chars(buf, buf + buf_size, v);
  *ptr = '\0';
}

inline void value_to_string(char* buf, std::size_t buf_size, double v)
{
  auto [ptr, ec] = std::to_chars(buf, buf + buf_size, v);
  *ptr = '\0';
}

inline void value_to_string(char* buf, std::size_t buf_size, int v)
{
  auto [ptr, ec] = std::to_chars(buf, buf + buf_size, v);
  *ptr = '\0';
}

inline void value_to_string(char* buf, std::size_t buf_size, unsigned int v)
{
  auto [ptr, ec] = std::to_chars(buf, buf + buf_size, v);
  *ptr = '\0';
}

inline void value_to_string(char* buf, std::size_t buf_size, int64_t v)
{
  auto [ptr, ec] = std::to_chars(buf, buf + buf_size, v);
  *ptr = '\0';
}

inline void value_to_string(char* buf, std::size_t buf_size, uint64_t v)
{
  auto [ptr, ec] = std::to_chars(buf, buf + buf_size, v);
  *ptr = '\0';
}

inline void value_to_string(char* buf, std::size_t buf_size, bool v)
{
  buf[0] = v ? '1' : '0';
  buf[1] = '\0';
}

template <typename T>
  requires avnd::string_ish<T>
inline void value_to_string(char* buf, std::size_t buf_size, const T& v)
{
  std::string_view sv{v};
  auto n = std::min(sv.size(), buf_size - 1);
  std::memcpy(buf, sv.data(), n);
  buf[n] = '\0';
}

template <typename T>
  requires std::is_enum_v<T>
inline void value_to_string(char* buf, std::size_t buf_size, T v)
{
  value_to_string(buf, buf_size, static_cast<int>(v));
}

// Fallback for anything else (structs, etc.) — not directly serializable
template <typename T>
  requires(!std::is_arithmetic_v<T> && !std::is_enum_v<T> && !avnd::string_ish<T>)
inline void value_to_string(char* buf, std::size_t buf_size, const T&)
{
  buf[0] = '\0';
}

/**
 * Traits to classify what goes to Info CHOP vs Info DAT.
 *
 * - Scalar numerics (float, int, bool, enum) → Info CHOP
 * - Containers of aggregates / containers of containers → Info DAT table
 * - Containers of scalars → Info DAT table (single column)
 * - Strings → Info DAT
 */

// Is this a "flat scalar" that maps to a single Info CHOP channel?
template <typename T>
concept info_chop_value = avnd::fp_ish<T> || avnd::int_ish<T> || avnd::bool_ish<T>
                          || std::is_enum_v<T>;

// Can we iterate the inner elements of a row to produce cells?
template <typename T>
concept info_dat_row_iterable = avnd::span_ish<T> || avnd::tuple_ish<T>;

// Is this a type whose .value is a container we can tabulate?
template <typename T>
concept has_iterable_value = requires(const T& t) {
  std::begin(t.value);
  std::end(t.value);
  std::size(t.value);
};

// Does this port have a scalar value suitable for Info CHOP?
template <typename T>
concept info_chop_port = avnd::parameter_port<T> && info_chop_value<decltype(T::value)>;

// Does this port have a container value suitable for Info DAT table?
template <typename T>
concept info_dat_port = has_iterable_value<T> && !info_chop_value<std::decay_t<decltype(T::value)>>;

// Does this port have a string value suitable for Info DAT?
template <typename T>
concept info_dat_string_port
    = avnd::parameter_port<T> && avnd::string_ish<std::decay_t<decltype(T::value)>>;

/**
 * Access the nth element of any iterable container.
 * Uses operator[] for random-access containers, iterator advancement otherwise.
 */
template <typename Container>
decltype(auto) nth_element(const Container& c, int n)
{
  if constexpr(requires { c[n]; })
  {
    return c[n];
  }
  else
  {
    auto it = std::begin(c);
    std::advance(it, n);
    return *it;
  }
}

/**
 * Count the number of leaf-level columns a single element expands to.
 * Recurses one level for pairs/tuples containing aggregates.
 */
template <typename Elem>
constexpr int columns_for_element();

template <typename Elem>
constexpr int columns_for_leaf()
{
  using E = std::decay_t<Elem>;
  if constexpr(std::is_aggregate_v<E> && !avnd::string_ish<E> && !avnd::span_ish<E>
               && !avnd::tuple_ish<E>)
    return avnd::pfr::tuple_size_v<E>;
  else
    return 1;
}

template <typename Elem>
constexpr int columns_for_element()
{
  using E = std::decay_t<Elem>;
  if constexpr(avnd::pair_ish<E>)
  {
    return columns_for_leaf<decltype(std::declval<E>().first)>()
           + columns_for_leaf<decltype(std::declval<E>().second)>();
  }
  else if constexpr(avnd::tuple_ish<E>)
  {
    return []<std::size_t... I>(std::index_sequence<I...>) {
      return (columns_for_leaf<std::tuple_element_t<I, E>>() + ...);
    }(std::make_index_sequence<std::tuple_size_v<E>>{});
  }
  else if constexpr(std::is_aggregate_v<E> && !avnd::string_ish<E>
                    && !avnd::span_ish<E>)
    return avnd::pfr::tuple_size_v<E>;
  else
    return 1; // scalar or string → single column
}

/**
 * Write a single leaf value (scalar, string, or flat aggregate) as DAT cells.
 * Returns the number of columns written.
 */
inline int write_leaf_cells(
    const auto& val, TD::OP_InfoDATEntries* entries, int col_offset)
{
  using V = std::decay_t<decltype(val)>;
  char buf[64];

  if constexpr(std::is_aggregate_v<V> && !avnd::string_ish<V>
               && !avnd::span_ish<V> && !avnd::tuple_ish<V>)
  {
    int col = col_offset;
    avnd::for_each_field_ref(val, [&](const auto& field) {
      value_to_string(buf, sizeof(buf), field);
      entries->values[col]->setString(buf);
      col++;
    });
    return col - col_offset;
  }
  else
  {
    value_to_string(buf, sizeof(buf), val);
    entries->values[col_offset]->setString(buf);
    return 1;
  }
}

/**
 * Write a single element's fields as DAT cells in a row.
 * Handles pairs, tuples, aggregates, and scalars.
 * For pairs/tuples, each member is expanded if it's an aggregate.
 */
template <typename Elem>
void write_element_cells(
    const Elem& elem, TD::OP_InfoDATEntries* entries, int col_offset)
{
  using E = std::decay_t<Elem>;

  if constexpr(avnd::pair_ish<E>)
  {
    int written = write_leaf_cells(elem.first, entries, col_offset);
    write_leaf_cells(elem.second, entries, col_offset + written);
  }
  else if constexpr(avnd::tuple_ish<E>)
  {
    int col = col_offset;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((col += write_leaf_cells(std::get<I>(elem), entries, col)), ...);
    }(std::make_index_sequence<std::tuple_size_v<E>>{});
  }
  else if constexpr(std::is_aggregate_v<E> && !avnd::string_ish<E> && !avnd::span_ish<E>)
  {
    write_leaf_cells(elem, entries, col_offset);
  }
  else
  {
    write_leaf_cells(elem, entries, col_offset);
  }
}

/**
 * Write column headers for a leaf type (scalar, string, or flat aggregate).
 * Returns the number of columns written.
 */
template <typename V>
int write_leaf_headers(
    TD::OP_InfoDATEntries* entries, int col_offset, std::string_view fallback_name)
{
  using E = std::decay_t<V>;
  if constexpr(std::is_aggregate_v<E> && !avnd::string_ish<E>
               && !avnd::span_ish<E> && !avnd::tuple_ish<E>)
  {
    constexpr int N = avnd::pfr::tuple_size_v<E>;
    if constexpr(avnd::has_field_names<E>)
    {
      [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((entries->values[col_offset + I]->setString(
             std::string(E::field_names()[I]).c_str())),
         ...);
      }(std::make_index_sequence<N>{});
    }
    else
    {
      [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((entries->values[col_offset + I]->setString(
             std::string(boost::pfr::get_name<I, E>()).c_str())),
         ...);
      }(std::make_index_sequence<N>{});
    }
    return N;
  }
  else
  {
    entries->values[col_offset]->setString(std::string(fallback_name).c_str());
    return 1;
  }
}

/**
 * Write column headers for an element type.
 * Expands aggregate members inside pairs/tuples.
 */
template <typename Elem>
void write_element_headers(
    TD::OP_InfoDATEntries* entries, int col_offset,
    [[maybe_unused]] std::string_view port_name)
{
  using E = std::decay_t<Elem>;

  if constexpr(avnd::pair_ish<E>)
  {
    using first_t = std::decay_t<decltype(std::declval<E>().first)>;
    using second_t = std::decay_t<decltype(std::declval<E>().second)>;
    int written = write_leaf_headers<first_t>(entries, col_offset, "key");
    write_leaf_headers<second_t>(entries, col_offset + written, "value");
  }
  else if constexpr(avnd::tuple_ish<E>)
  {
    int col = col_offset;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((col += write_leaf_headers<std::tuple_element_t<I, E>>(
            entries, col,
            std::string_view{"col"} /* fallback, gets index appended below */)),
       ...);
    }(std::make_index_sequence<std::tuple_size_v<E>>{});
  }
  else if constexpr(std::is_aggregate_v<E> && !avnd::string_ish<E> && !avnd::span_ish<E>)
  {
    write_leaf_headers<E>(entries, col_offset, port_name);
  }
  else
  {
    entries->values[col_offset]->setString(std::string(port_name).c_str());
  }
}

/**
 * Count columns for a container of container (vector<vector<T>>).
 * Returns 0 if we can't determine columns at compile time (dynamic).
 */
template <typename Container>
int runtime_max_inner_size(const Container& c)
{
  int max_size = 0;
  for(const auto& inner : c)
  {
    int sz = 0;
    if constexpr(avnd::span_ish<std::decay_t<decltype(inner)>>)
      sz = static_cast<int>(std::size(inner));
    else
      sz = columns_for_element<decltype(inner)>();
    if(sz > max_size)
      max_size = sz;
  }
  return max_size;
}

/**
 * info_output<T, PrimaryPred>
 *
 * Shared utility for all TD operator bindings.
 * Routes non-primary parameter outputs to Info CHOP and Info DAT.
 *
 * Template parameters:
 *   T            - The Avendish processor type
 *   PrimaryPred  - A concept/predicate that identifies which output ports
 *                  are the primary output for this operator type.
 *                  E.g., for TOP: avnd::cpu_texture_port or avnd::gpu_texture_port
 *                  Ports matching PrimaryPred are skipped.
 *
 * Usage in a binding:
 *   info_output<T> info;  // for operators where all parameter outputs are secondary
 *   // then delegate getNumInfoCHOPChans, getInfoCHOPChan, etc. to info.*
 */
template <typename T>
struct info_output
{
  // ===== Info CHOP: scalar numeric outputs =====

  static int32_t get_num_info_chop_chans(avnd::effect_container<T>& impl)
  {
    if constexpr(!avnd::has_outputs<T>)
      return 0;
    else
    {
      int32_t count = 0;
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(impl),
          [&]<typename Field>(Field& field) {
        if constexpr(info_chop_value<std::decay_t<decltype(field.value)>>)
          count++;
      });
      return count;
    }
  }

  static void get_info_chop_chan(
      avnd::effect_container<T>& impl,
      int32_t index, TD::OP_InfoCHOPChan* chan)
  {
    if constexpr(!avnd::has_outputs<T>)
      return;
    else
    {
      int32_t current = 0;
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(impl),
          [&]<typename Field>(Field& field) {
        if constexpr(info_chop_value<std::decay_t<decltype(field.value)>>)
        {
          if(current == index)
          {
            static constexpr auto name = get_td_name<Field>();
            chan->name->setString(name.data());

            if constexpr(avnd::fp_ish<decltype(field.value)>)
              chan->value = static_cast<float>(field.value);
            else if constexpr(avnd::int_ish<decltype(field.value)>)
              chan->value = static_cast<float>(field.value);
            else if constexpr(avnd::bool_ish<decltype(field.value)>)
              chan->value = field.value ? 1.0f : 0.0f;
            else if constexpr(std::is_enum_v<decltype(field.value)>)
              chan->value = static_cast<float>(static_cast<int>(field.value));
          }
          current++;
        }
      });
    }
  }

  // ===== Info DAT: tabular and string outputs =====

  // Compute total rows and columns across all Info DAT-eligible outputs.
  // Layout: each container output gets its own column group.
  // Rows = max container size + 1 (header). String outputs add 1 row each.
  struct dat_layout
  {
    int32_t rows = 0;
    int32_t cols = 0;
    int num_table_ports = 0;
    int num_string_ports = 0;
  };

  static dat_layout compute_dat_layout(avnd::effect_container<T>& impl)
  {
    if constexpr(!avnd::has_outputs<T>)
      return {};

    dat_layout layout{};
    int max_rows = 0;
    int total_cols = 0;

    avnd::parameter_output_introspection<T>::for_all(
        avnd::get_outputs(impl),
        [&]<typename Field>(Field& field) {
      if constexpr(info_dat_port<Field>)
      {
        int container_size = static_cast<int>(std::size(field.value));
        if(container_size > max_rows)
          max_rows = container_size;

        using value_type = std::decay_t<decltype(field.value)>;
        using elem_type = typename value_type::value_type;

        // Determine columns for this port
        if constexpr(avnd::span_ish<std::decay_t<elem_type>>)
        {
          // Container of containers: need runtime scan for max inner size
          total_cols += runtime_max_inner_size(field.value);
        }
        else
        {
          total_cols += columns_for_element<elem_type>();
        }
        layout.num_table_ports++;
      }
      else if constexpr(info_dat_string_port<Field>)
      {
        layout.num_string_ports++;
      }
    });

    if(layout.num_table_ports > 0)
    {
      layout.rows = max_rows + 1; // +1 for header
      layout.cols = total_cols;
    }

    // String outputs: append as additional rows (key-value)
    if(layout.num_string_ports > 0 && layout.num_table_ports == 0)
    {
      layout.rows = layout.num_string_ports;
      layout.cols = 2; // name, value
    }
    else if(layout.num_string_ports > 0)
    {
      // Mix of tables and strings: just add to the table rows
      layout.rows += layout.num_string_ports;
    }

    return layout;
  }

  static bool get_info_dat_size(
      avnd::effect_container<T>& impl, TD::OP_InfoDATSize* infoSize)
  {
    auto layout = compute_dat_layout(impl);
    if(layout.rows == 0 || layout.cols == 0)
      return false;

    infoSize->rows = layout.rows;
    infoSize->cols = layout.cols;
    infoSize->byColumn = false;
    return true;
  }

  static void get_info_dat_entries(
      avnd::effect_container<T>& impl,
      int32_t row_index, int32_t nEntries, TD::OP_InfoDATEntries* entries)
  {
    if constexpr(!avnd::has_outputs<T>)
      return;

    auto layout = compute_dat_layout(impl);

    if(layout.num_table_ports > 0)
    {
      write_table_entries(impl, row_index, nEntries, entries, layout);
    }
    else if(layout.num_string_ports > 0)
    {
      write_string_entries(impl, row_index, nEntries, entries);
    }
  }

private:
  // Write table entries (header row + data rows from container outputs)
  static void write_table_entries(
      avnd::effect_container<T>& impl,
      int32_t row_index, int32_t nEntries,
      TD::OP_InfoDATEntries* entries,
      const dat_layout& layout)
  {
    if(row_index == 0)
    {
      // Header row: column names
      int col_offset = 0;
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(impl),
          [&]<typename Field>(Field& field) {
        if constexpr(info_dat_port<Field>)
        {
          using value_type = std::decay_t<decltype(field.value)>;
          using elem_type = typename value_type::value_type;

          const auto port_name = avnd::get_name<Field>();

          if constexpr(avnd::span_ish<std::decay_t<elem_type>>)
          {
            // Container of containers: numbered columns
            int ncols = runtime_max_inner_size(field.value);
            char buf[32];
            for(int i = 0; i < ncols; ++i)
            {
              snprintf(buf, sizeof(buf), "%d", i);
              if(col_offset + i < nEntries)
                entries->values[col_offset + i]->setString(buf);
            }
            col_offset += ncols;
          }
          else
          {
            int ncols = columns_for_element<elem_type>();
            if(col_offset + ncols <= nEntries)
              write_element_headers<elem_type>(entries, col_offset, port_name);
            col_offset += ncols;
          }
        }
      });
    }
    else
    {
      // Data row
      int data_row = row_index - 1;
      int col_offset = 0;

      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(impl),
          [&]<typename Field>(Field& field) {
        if constexpr(info_dat_port<Field>)
        {
          using value_type = std::decay_t<decltype(field.value)>;
          using elem_type = typename value_type::value_type;

          int container_size = static_cast<int>(std::size(field.value));

          if constexpr(avnd::span_ish<std::decay_t<elem_type>>)
          {
            // Container of containers (e.g., vector<vector<float>>)
            int ncols = runtime_max_inner_size(field.value);

            if(data_row < container_size)
            {
              const auto& inner = nth_element(field.value, data_row);
              int inner_size = static_cast<int>(std::size(inner));
              char buf[64];

              for(int c = 0; c < ncols && (col_offset + c) < nEntries; ++c)
              {
                if(c < inner_size)
                {
                  value_to_string(buf, sizeof(buf), nth_element(inner, c));
                  entries->values[col_offset + c]->setString(buf);
                }
                else
                {
                  entries->values[col_offset + c]->setString("");
                }
              }
            }
            else
            {
              // Past this container's data: empty cells
              for(int c = 0; c < ncols && (col_offset + c) < nEntries; ++c)
                entries->values[col_offset + c]->setString("");
            }
            col_offset += ncols;
          }
          else
          {
            // Container of scalars/aggregates/tuples
            int ncols = columns_for_element<elem_type>();

            if(data_row < container_size)
            {
              write_element_cells(nth_element(field.value, data_row), entries, col_offset);
            }
            else
            {
              for(int c = 0; c < ncols && (col_offset + c) < nEntries; ++c)
                entries->values[col_offset + c]->setString("");
            }
            col_offset += ncols;
          }
        }
      });
    }
  }

  // Write string entries as key-value pairs (name column + value column)
  static void write_string_entries(
      avnd::effect_container<T>& impl,
      int32_t row_index, int32_t nEntries,
      TD::OP_InfoDATEntries* entries)
  {
    int current = 0;
    avnd::parameter_output_introspection<T>::for_all(
        avnd::get_outputs(impl),
        [&]<typename Field>(Field& field) {
      if constexpr(info_dat_string_port<Field>)
      {
        if(current == row_index)
        {
          static constexpr auto name = get_td_name<Field>();
          if(nEntries > 0)
            entries->values[0]->setString(name.data());
          if(nEntries > 1)
          {
            if constexpr(avnd::string_ish<std::decay_t<decltype(field.value)>>)
            {
              std::string_view sv{field.value};
              entries->values[1]->setString(std::string(sv).c_str());
            }
          }
        }
        current++;
      }
    });
  }
};

}
