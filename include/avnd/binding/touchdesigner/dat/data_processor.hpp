#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/common/export.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_double.hpp>

#include <DAT_CPlusPlusBase.h>

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

  // Info methods
  int32_t getNumInfoCHOPChans(void* reserved) override { return 0; }

  void getInfoCHOPChan(int32_t index, TD::OP_InfoCHOPChan* chan, void* reserved) override
  {
  }

  bool getInfoDATSize(TD::OP_InfoDATSize* infoSize, void* reserved) override
  {
    return false;
  }

  void getInfoDATEntries(
      int32_t index, int32_t nEntries, TD::OP_InfoDATEntries* entries,
      void* reserved) override
  {
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
      double val{};
      if(auto res = std::from_chars(str, str + strlen(str), val); res.ec == std::errc{})
        return val;
      else
        return 0.;
    }
    else if constexpr(avnd::bool_ish<value_type>)
    {
      bool val;
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
      }
      // TODO do it in a more advanced way, like Max and ossia
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
      // FIXME if we're generating arrays we likely want one value per cell
      // Create a table with 1 column per output
      // FIXME find max array length and use that as column size
      output->setOutputDataType(TD::DAT_OutDataType::Table);
      output->setTableSize(num_outputs, 1);

      // Fill the table with output values
      int row_index = 0;
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&]<typename Field>(Field& field) {
        write_cell(output, row_index, 0, field.value);
        row_index++;
      });
    }
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
