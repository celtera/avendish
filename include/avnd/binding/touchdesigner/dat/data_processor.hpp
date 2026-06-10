#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <DAT_CPlusPlusBase.h>
#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/binding/touchdesigner/info_output.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/tensor.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_double.hpp>

#include <charconv>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
namespace touchdesigner::DAT
{

/**
 * Data processor binding for TouchDesigner DATs
 * Maps Avendish message/data processors to TD's DAT API
 *
 * Inspired by CHOP message_processor pattern
 * Handles table-based data (rows/columns of strings, ints, doubles)
 */
template <typename T>
struct data_processor : public TD::DAT_CPlusPlusBase
{
  avnd::effect_container<T> implementation;
  parameter_setup<T> param_setup;

  explicit data_processor(const TD::OP_NodeInfo* info)
  {
    init();
  }

  void init()
  {
    avnd::init_controls(implementation);
  }

  void getGeneralInfo(
      TD::DAT_GeneralInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // Cook every frame if we have no outputs (like network senders)
    info->cookEveryFrame = (avnd::output_introspection<T>::size == 0);

    // Cook if asked for data
    info->cookEveryFrameIfAsked = true;
  }

  void execute(TD::DAT_Output* output, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // Update parameter values from TouchDesigner
    update_controls(inputs);

    // Process input DATs if any
    if constexpr(avnd::value_port_input_introspection<T>::size > 0)
    {
      int input_index = 0;
      avnd::value_port_input_introspection<T>::for_all(
          avnd::get_inputs(implementation),
          [&]<typename Field>(Field& field) {
            if(input_index < inputs->getNumInputs())
            {
              const TD::OP_DATInput* dat_input = inputs->getInputDAT(input_index);
              if(dat_input)
              {
                read_dat_input(dat_input, field);
              }
            }
            input_index++;
          });
    }

    // Execute the processor
    struct tick
    {
      int frames = 0;
    } t{.frames = 1};

    invoke_effect(implementation, t);

    // Generate output - table or text based on processor type
    if constexpr(avnd::has_outputs<T>)
    {
      write_dat_output(output);
    }
    else
    {
      // No outputs - just set empty table
      output->setOutputDataType(TD::DAT_OutDataType::Table);
      output->setTableSize(0, 0);
    }
  }

  void setupParameters(TD::OP_ParameterManager* manager, void* reserved) override
  {
    param_setup.setup(implementation, manager);
  }

  void pulsePressed(const char* name, void* reserved) override
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.pulse(implementation, name);
  }

  void buildDynamicMenu(
      const TD::OP_Inputs* inputs, TD::OP_BuildDynamicMenuInfo* info,
      void* reserved) override
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.menu(implementation, inputs, info);
  }

  // Info methods — route non-primary outputs to Info CHOP/DAT
  int32_t getNumInfoCHOPChans(void* reserved) override
  {
    return info_output<T>::get_num_info_chop_chans(implementation);
  }

  void getInfoCHOPChan(int32_t index, TD::OP_InfoCHOPChan* chan, void* reserved) override
  {
    info_output<T>::get_info_chop_chan(implementation, index, chan);
  }

  bool getInfoDATSize(TD::OP_InfoDATSize* infoSize, void* reserved) override
  {
    return info_output<T>::get_info_dat_size(implementation, infoSize);
  }

  void getInfoDATEntries(
      int32_t index, int32_t nEntries, TD::OP_InfoDATEntries* entries,
      void* reserved) override
  {
    info_output<T>::get_info_dat_entries(implementation, index, nEntries, entries);
  }

  void getWarningString(TD::OP_String* warning, void* reserved) override {}

  void getErrorString(TD::OP_String* error, void* reserved) override {}

  void getInfoPopupString(TD::OP_String* info, void* reserved) override {}

private:
  // Read input DAT into Avendish port
  template<typename value_type>
  auto read_dat_cell(const char* str)
  {
    if constexpr(avnd::string_ish<value_type>)
    {
      if(str)
        return str;
      else
        return value_type{};
    }
    else if constexpr(avnd::int_ish<value_type>)
    {
      int32_t val{};
      if(auto res = std::from_chars(str, str + strlen(str), val); res.ec == std::errc{})
        return val;
      else
        return 0;
    }
    else if constexpr(avnd::fp_ish<value_type>)
    {
      // Apple's libc++ does not provide floating-point std::from_chars
      char* end{};
      if(double val = std::strtod(str, &end); end != str)
        return val;
      else
        return 0.;
    }
    else if constexpr(avnd::bool_ish<value_type>)
    {
      int val{};
      if(auto res = std::from_chars(str, str + strlen(str), val); res.ec == std::errc{})
        return static_cast<value_type>(val);
      else if(std::strcmp(str, "true") == 0)
        return true;
      else if(std::strcmp(str, "True") == 0)
        return true;
      else
        return false;
    }
    else
    {
      return value_type{};
    }
  }

  template <typename Field>
  void read_dat_input(const TD::OP_DATInput* dat_input, Field& field)
  {
    // For now, we read the DAT as a simple value from cell (0,0)
    // More sophisticated handling could read entire tables
    if constexpr(requires { field.value; })
    {
      using value_type = std::decay_t<decltype(field.value)>;

      if constexpr(avnd::string_ish<value_type>)
      {
        // Read as string
        if(const char* str = dat_input->getCell(0, 0))
          field.value = read_dat_cell<value_type>(str);
      }
      else if constexpr(avnd::int_ish<value_type>)
      {
        if(const char* str = dat_input->getCell(0, 0))
          field.value = read_dat_cell<value_type>(str);
      }
      else if constexpr(avnd::fp_ish<value_type>)
      {
        if(const char* str = dat_input->getCell(0, 0))
          field.value = read_dat_cell<value_type>(str);
      }
      else if constexpr(avnd::vector_ish<value_type>)
      {
        using vec_val_type = typename value_type::value_type;
        field.value.clear();
        field.value.resize(dat_input->numCols);
        for(int i = 0; i < dat_input->numCols; i++) {
          if(const char* str = dat_input->getCell(0, i))
            field.value[i] = read_dat_cell<vec_val_type>(str);
        }
      }
      else if constexpr(avnd::static_array_ish<value_type>)
      {
        using arr_val_type = std::decay_t<decltype(field.value[0])>;
        const int N = std::min((int)dat_input->numCols, (int)std::tuple_size_v<value_type>);
        for(int i = 0; i < N; i++) {
          if(const char* str = dat_input->getCell(0, i))
            field.value[i] = read_dat_cell<value_type>(str);
          else
            field.value[i] = {};
        }
      }
      else if constexpr(avnd::pair_ish<value_type>)
      {
        using arr_val_type = std::decay_t<decltype(field.value.first)>;
        const int N = std::min((int)dat_input->numCols, 2);
        if(const char* str = dat_input->getCell(0, 0))
          field.value.first = read_dat_cell<arr_val_type>(str);
        if(const char* str = dat_input->getCell(0, 1))
          field.value.second = read_dat_cell<arr_val_type>(str);
      }
      else if constexpr(avnd::tensor_like<value_type>)
      {
        read_dat_input_tensor<Field>(dat_input, field);
      }
    }
    // TODO do it in a more advanced way, like Max and ossia
  }

  template <typename Field>
  void read_dat_input_tensor(const TD::OP_DATInput* dat_input, Field& field)
  {
    using value_type = std::remove_cvref_t<decltype(field.value)>;
    using elem_t = avnd::tensor_element<value_type>;
    constexpr std::size_t static_rank = avnd::static_port_rank<Field>();

    const int rows = dat_input->numRows;
    const int cols = dat_input->numCols;
    if(rows <= 0 || cols <= 0)
      return;

    constexpr bool wire_rank_1 = (static_rank == 1);

    if constexpr(avnd::view_tensor_like<value_type>)
    {
      if constexpr(wire_rank_1)
      {
        const std::size_t N = static_cast<std::size_t>(cols);
        auto buf = std::shared_ptr<elem_t[]>(new elem_t[N]);
        for(int c = 0; c < cols; ++c)
        {
          if(const char* str = dat_input->getCell(0, c))
            buf[c] = read_dat_cell<elem_t>(str);
          else
            buf[c] = elem_t{};
        }
        avnd::set_view_buffer(
            field.value, buf.get(), std::vector<std::size_t>{N},
            std::shared_ptr<void>(buf, buf.get()));
      }
      else
      {
        const std::size_t R = static_cast<std::size_t>(rows);
        const std::size_t C = static_cast<std::size_t>(cols);
        const std::size_t N = R * C;
        auto buf = std::shared_ptr<elem_t[]>(new elem_t[N]);
        for(int r = 0; r < rows; ++r)
        {
          for(int c = 0; c < cols; ++c)
          {
            if(const char* str = dat_input->getCell(r, c))
              buf[r * C + c] = read_dat_cell<elem_t>(str);
            else
              buf[r * C + c] = elem_t{};
          }
        }
        avnd::set_view_buffer(
            field.value, buf.get(), std::vector<std::size_t>{R, C},
            std::shared_ptr<void>(buf, buf.get()));
      }
    }
    else if constexpr(avnd::resizable_tensor_like<value_type>)
    {
      if constexpr(wire_rank_1)
      {
        std::vector<std::size_t> shape{static_cast<std::size_t>(cols)};
        avnd::resize_to(field.value, shape);
        elem_t* data = avnd::data_of(field.value);
        for(int c = 0; c < cols; ++c)
        {
          if(const char* str = dat_input->getCell(0, c))
            data[c] = read_dat_cell<elem_t>(str);
          else
            data[c] = elem_t{};
        }
      }
      else
      {
        std::vector<std::size_t> shape{
            static_cast<std::size_t>(rows), static_cast<std::size_t>(cols)};
        avnd::resize_to(field.value, shape);
        elem_t* data = avnd::data_of(field.value);
        const std::size_t C = static_cast<std::size_t>(cols);
        for(int r = 0; r < rows; ++r)
        {
          for(int c = 0; c < cols; ++c)
          {
            if(const char* str = dat_input->getCell(r, c))
              data[r * C + c] = read_dat_cell<elem_t>(str);
            else
              data[r * C + c] = elem_t{};
          }
        }
      }
    }
    else
    {
      const auto shape = avnd::shape_of(field.value);
      elem_t* data = avnd::data_of(field.value);
      if(data == nullptr)
        return;
      const std::size_t srank = std::ranges::size(shape);
      auto it = std::ranges::begin(shape);
      if(srank == 1)
      {
        const std::size_t N = static_cast<std::size_t>(*it);
        const int lim = std::min(cols, static_cast<int>(N));
        for(int c = 0; c < lim; ++c)
        {
          if(const char* str = dat_input->getCell(0, c))
            data[c] = read_dat_cell<elem_t>(str);
          else
            data[c] = elem_t{};
        }
      }
      else if(srank >= 2)
      {
        const std::size_t R = static_cast<std::size_t>(*it);
        ++it;
        const std::size_t C = static_cast<std::size_t>(*it);
        const int rlim = std::min(rows, static_cast<int>(R));
        const int clim = std::min(cols, static_cast<int>(C));
        for(int r = 0; r < rlim; ++r)
        {
          for(int c = 0; c < clim; ++c)
          {
            if(const char* str = dat_input->getCell(r, c))
              data[r * C + c] = read_dat_cell<elem_t>(str);
            else
              data[r * C + c] = elem_t{};
          }
        }
      }
    }
  }

  // Write Avendish outputs to DAT
  void write_dat_output(TD::DAT_Output* output)
  {
    if constexpr(avnd::output_introspection<T>::size == 0)
    {
      output->setOutputDataType(TD::DAT_OutDataType::Table);
      output->setTableSize(0, 0);
      return;
    }

    // Count outputs to determine table size
    constexpr int num_outputs = avnd::output_introspection<T>::size;
    if constexpr(num_outputs == 1)
    {
      auto& first_field = avnd::output_introspection<T>::template field<0>(avnd::get_outputs(implementation));
      using value_type = std::decay_t<decltype(first_field.value)>;
      if constexpr(avnd::string_ish<value_type>)
      {
        output->setOutputDataType(TD::DAT_OutDataType::Text);
        output->setText(first_field.value.c_str());
      }
      else if constexpr(avnd::vector_ish<value_type>)
      {
        output->setOutputDataType(TD::DAT_OutDataType::Table);
        const auto N = first_field.value.size();
        output->setTableSize(1, N);
        for(int col_index = 0; col_index < N; col_index++) {
          write_cell(output, 0, col_index, first_field.value[col_index]);
        }
      }
      else if constexpr(avnd::static_array_ish<value_type>)
      {
        output->setOutputDataType(TD::DAT_OutDataType::Table);
        const int N = std::ssize(first_field.value);
        output->setTableSize(1, N);
        for(int col_index = 0; col_index < N; col_index++) {
          write_cell(output, 0, col_index, first_field.value[col_index]);
        }
      }
      else if constexpr(avnd::tensor_like<value_type>)
      {
        output->setOutputDataType(TD::DAT_OutDataType::Table);
        write_tensor_to_table<std::decay_t<decltype(first_field)>>(
            output, first_field.value);
      }
      else
      {
        // FIXME boost::multiarray, mdspan, mdarray, vector<vector<...>> etc.
        // FIXME if we're generating arrays we likely want one value per cell
        // Create a table with 1 column per output
        // FIXME find max array length and use that as column size
        output->setOutputDataType(TD::DAT_OutDataType::Table);
        output->setTableSize(1, 1);
        write_cell(output, 0, 0, first_field.value);
      }
    }
    else
    {
      std::size_t max_cols = 1;
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&]<typename Field>(Field& field) {
            using vt = std::decay_t<decltype(field.value)>;
            if constexpr(avnd::tensor_like<vt>)
            {
              const auto sh = avnd::shape_of(field.value);
              std::size_t total = 1;
              std::size_t header_cols = 0;
              std::size_t r0 = 0;
              std::size_t i = 0;
              for(auto e : sh)
              {
                if(i == 0) r0 = static_cast<std::size_t>(e);
                total *= static_cast<std::size_t>(e);
                ++i;
              }
              const std::size_t rank = i;
              std::size_t need_cols = 1;
              if(rank == 1) need_cols = total;
              else if(rank == 2 && r0 > 0) need_cols = total / r0;
              else { header_cols = rank; need_cols = total; }
              std::size_t mc = std::max(need_cols, header_cols);
              if(mc > max_cols) max_cols = mc;
            }
          });

      output->setOutputDataType(TD::DAT_OutDataType::Table);
      output->setTableSize(num_outputs, static_cast<int32_t>(max_cols));

      int row_index = 0;
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&]<typename Field>(Field& field) {
            using vt = std::decay_t<decltype(field.value)>;
            if constexpr(avnd::tensor_like<vt>)
            {
              write_tensor_row(output, row_index, field.value, max_cols);
            }
            else
            {
              write_cell(output, row_index, 0, field.value);
              for(std::size_t c = 1; c < max_cols; ++c)
                output->setCellString(row_index, static_cast<int32_t>(c), "");
            }
            row_index++;
          });
    }
  }

  template <typename Field, typename Container>
  void write_tensor_to_table(TD::DAT_Output* output, const Container& v)
  {
    using elem_t = avnd::tensor_element<Container>;
    const auto shape = avnd::shape_of(v);
    const elem_t* data = avnd::data_of(v);
    const std::size_t rank = std::ranges::size(shape);
    constexpr std::size_t static_rank = avnd::static_port_rank<Field>();

    if(data == nullptr || rank == 0)
    {
      output->setTableSize(0, 0);
      return;
    }

    std::size_t total = 1;
    std::size_t d0 = 0, d1 = 0;
    std::size_t i = 0;
    for(auto e : shape)
    {
      const auto se = static_cast<std::size_t>(e);
      total *= se;
      if(i == 0) d0 = se;
      else if(i == 1) d1 = se;
      ++i;
    }

    if constexpr(static_rank == 1)
    {
      output->setTableSize(1, static_cast<int32_t>(d0));
      for(std::size_t c = 0; c < d0; ++c)
        write_cell(output, 0, static_cast<int32_t>(c), data[c]);
    }
    else if constexpr(static_rank == 2)
    {
      output->setTableSize(static_cast<int32_t>(d0), static_cast<int32_t>(d1));
      for(std::size_t r = 0; r < d0; ++r)
        for(std::size_t c = 0; c < d1; ++c)
          write_cell(
              output, static_cast<int32_t>(r), static_cast<int32_t>(c),
              data[r * d1 + c]);
    }
    else
    {
      if(rank == 1)
      {
        output->setTableSize(1, static_cast<int32_t>(d0));
        for(std::size_t c = 0; c < d0; ++c)
          write_cell(output, 0, static_cast<int32_t>(c), data[c]);
      }
      else if(rank == 2)
      {
        output->setTableSize(
            static_cast<int32_t>(d0), static_cast<int32_t>(d1));
        for(std::size_t r = 0; r < d0; ++r)
          for(std::size_t c = 0; c < d1; ++c)
            write_cell(
                output, static_cast<int32_t>(r), static_cast<int32_t>(c),
                data[r * d1 + c]);
      }
      else
      {
        // Rank-erased: shape header on row 0, flat values on row 1.
        const std::size_t cols = std::max(rank, total);
        output->setTableSize(2, static_cast<int32_t>(cols));
        std::size_t j = 0;
        for(auto e : shape)
        {
          output->setCellInt(
              0, static_cast<int32_t>(j),
              static_cast<int32_t>(static_cast<std::size_t>(e)));
          ++j;
        }
        for(std::size_t k = j; k < cols; ++k)
          output->setCellString(0, static_cast<int32_t>(k), "");
        for(std::size_t k = 0; k < total; ++k)
          write_cell(output, 1, static_cast<int32_t>(k), data[k]);
        for(std::size_t k = total; k < cols; ++k)
          output->setCellString(1, static_cast<int32_t>(k), "");
      }
    }
  }

  template <typename Container>
  void write_tensor_row(
      TD::DAT_Output* output, int row, const Container& v, std::size_t row_cols)
  {
    using elem_t = avnd::tensor_element<Container>;
    const auto shape = avnd::shape_of(v);
    const elem_t* data = avnd::data_of(v);
    if(data == nullptr)
    {
      for(std::size_t c = 0; c < row_cols; ++c)
        output->setCellString(row, static_cast<int32_t>(c), "");
      return;
    }
    std::size_t total = 1;
    for(auto e : shape)
      total *= static_cast<std::size_t>(e);
    const std::size_t lim = std::min(total, row_cols);
    for(std::size_t c = 0; c < lim; ++c)
      write_cell(output, row, static_cast<int32_t>(c), data[c]);
    for(std::size_t c = lim; c < row_cols; ++c)
      output->setCellString(row, static_cast<int32_t>(c), "");
  }

  // Write a single cell value
  template <typename value_type>
  void write_cell(TD::DAT_Output* output, int row, int col, const value_type& value)
  {
    if constexpr(avnd::string_ish<value_type>)
    {
      output->setCellString(row, col, value.c_str());
    }
    else if constexpr(avnd::bool_ish<value_type>)
    {
      output->setCellInt(row, col, value ? 1 : 0);
    }
    else if constexpr(avnd::int_ish<value_type>)
    {
      output->setCellInt(row, col, static_cast<int32_t>(value));
    }
    else if constexpr(avnd::fp_ish<value_type>)
    {
      output->setCellDouble(row, col, value);
    }
    else
    {
      // Try to convert to string
      output->setCellString(row, col, std::to_string(value).c_str());
    }
  }

  void update_controls(const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.update(implementation, inputs);
  }
};

} // namespace touchdesigner::DAT
