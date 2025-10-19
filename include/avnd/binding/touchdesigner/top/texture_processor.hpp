#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>

#include <TOP_CPlusPlusBase.h>

namespace touchdesigner::TOP
{

/**
 * Texture processor binding for TouchDesigner TOPs
 * Maps Avendish texture processors to TD's TOP API
 */
template <typename T>
struct texture_processor : public TD::TOP_CPlusPlusBase
{
  avnd::effect_container<T> implementation;
  parameter_setup<T> param_setup;
  TD::TOP_Context* context{};

  // Track if we need to process
  bool needs_cook{true};

  explicit texture_processor(const TD::OP_NodeInfo* info, TD::TOP_Context* ctx)
      : context{ctx}
  {
    init();
  }

  void init()
  {
    avnd::init_controls(implementation);
  }

  void getGeneralInfo(
      TD::TOP_GeneralInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // Cook every frame if asked to ensure textures stay updated
    info->cookEveryFrameIfAsked = true;

    // If we have no inputs, we're a generator - cook every frame
    info->cookEveryFrame = (avnd::matrix_input_introspection<T>::size == 0);

    // Use first input for size reference by default
    info->inputSizeIndex = 0;
  }

  void execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // Update parameter values from TouchDesigner
    update_controls(inputs);

    // Handle texture inputs - download from GPU to CPU
    if constexpr(avnd::matrix_input_introspection<T>::size > 0)
    {
      int input_index = 0;
      avnd::matrix_input_introspection<T>::for_all(
          avnd::get_inputs(implementation),
          [&]<typename Field>(Field& field) {
            if(input_index < inputs->getNumInputs())
            {
              const TD::OP_TOPInput* top_input = inputs->getInputTOP(input_index);
              if(top_input && top_input->textureDesc.width > 0 && top_input->textureDesc.height > 0)
              {
                download_matrix(top_input, field);
              }
            }
            input_index++;
          });
    }

    // Execute the processor
    struct tick {
      int frames = 0;
    } t{.frames = 1};

    invoke_effect(implementation, t);

    // Handle texture outputs - upload from CPU to GPU
    if constexpr(avnd::matrix_output_introspection<T>::size > 0)
    {
      int output_index = 0;
      avnd::matrix_output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&]<typename Field>(Field& field) {
            if(output_index == 0)  // TD TOPs have single output
            {
              upload_matrix(output, field);
            }
            output_index++;
          });
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
  // Download texture from TD to Avendish texture port
  template <avnd::cpu_texture_port Field>
  void download_matrix(const TD::OP_TOPInput* top_input, Field& field)
  {
    auto& tex = field.texture;

    // Set up download options
    TD::OP_TOPInputDownloadOptions opts;
    opts.pixelFormat = top_input->textureDesc.pixelFormat;

    // Download the texture from GPU
    TD::OP_SmartRef<TD::OP_TOPDownloadResult> download_result =
        top_input->downloadTexture(opts, nullptr);

    if(!download_result)
      return;

    // Update Avendish texture structure
    tex.width = download_result->textureDesc.width;
    tex.height = download_result->textureDesc.height;
    tex.bytes = static_cast<unsigned char*>(download_result->getData());
    tex.changed = true;

    // Map TD pixel format to Avendish format
    if constexpr(requires { tex.format; })
    {
      tex.format = map_pixel_format(download_result->textureDesc.pixelFormat);
    }

    // Keep the download result alive (stored as member if needed)
    // For now, we process immediately so the temp reference is fine
  }

  // Upload texture from Avendish to TD
  template <avnd::cpu_texture_port Field>
  void upload_matrix(TD::TOP_Output* output, Field& field)
  {
    auto& tex = field.texture;

    // Skip if texture hasn't changed or is invalid
    if(!tex.changed || !tex.bytes || tex.width <= 0 || tex.height <= 0)
      return;

    // Calculate buffer size
    int bytes_per_pixel = get_bytes_per_pixel(tex);
    uint64_t buffer_size = static_cast<uint64_t>(tex.width) * tex.height * bytes_per_pixel;

    // Create output buffer
    TD::OP_SmartRef<TD::TOP_Buffer> out_buffer =
        context->createOutputBuffer(buffer_size, TD::TOP_BufferFlags::None, nullptr);

    if(!out_buffer)
      return;

    // Copy texture data to buffer
    std::memcpy(out_buffer->data, tex.bytes, buffer_size);

    // Set up upload info
    TD::TOP_UploadInfo upload_info;
    upload_info.textureDesc.width = tex.width;
    upload_info.textureDesc.height = tex.height;
    upload_info.textureDesc.texDim = TD::OP_TexDim::e2D;
    upload_info.textureDesc.pixelFormat = map_to_td_pixel_format(tex);
    upload_info.textureDesc.aspectX = tex.width;
    upload_info.textureDesc.aspectY = tex.height;
    upload_info.firstPixel = TD::TOP_FirstPixel::BottomLeft;
    upload_info.colorBufferIndex = 0;

    // Upload to TouchDesigner
    output->uploadBuffer(&out_buffer, upload_info, nullptr);

    // Mark as processed
    tex.changed = false;
  }

  template <avnd::cpu_buffer_port Field>
  void download_matrix(const TD::OP_TOPInput* top_input, Field& field)
  {
    // TODO
  }

  template <avnd::cpu_buffer_port Field>
  void upload_matrix(TD::TOP_Output* output, Field& field)
  {
    // TODO
  }

  // Helper to get bytes per pixel from Avendish texture
  template <typename Tex>
  int get_bytes_per_pixel(const Tex& tex)
  {
    if constexpr(requires { tex.format; })
    {
      // Dynamic format
      using format_type = std::decay_t<decltype(tex.format)>;
      if constexpr(std::is_integral_v<format_type>)
      {
        // Assume RGBA8 as default
        return 4;
      }
      else
      {
        // Try to deduce from format type
        return 4;
      }
    }
    else if constexpr(requires { typename Tex::format; })
    {
      // Fixed format type
      using format_type = typename Tex::format;

      // Check for common format types
      if constexpr(requires { format_type::bytes_per_pixel; })
        return format_type::bytes_per_pixel;
      else
        return 4; // RGBA8 default
    }
    else
    {
      // Default to RGBA8
      return 4;
    }
  }

  // Map Avendish format to TD pixel format
  TD::OP_PixelFormat map_to_td_pixel_format(const auto& tex)
  {
    // Default to RGBA8Fixed - most common format
    // Could be extended to detect format from texture metadata
    return TD::OP_PixelFormat::RGBA8Fixed;
  }

  // Map TD pixel format to Avendish format enum
  int map_pixel_format(TD::OP_PixelFormat td_format)
  {
    // Return a generic format identifier
    // Specific mappings would depend on the Avendish texture format enum
    return 0; // RGBA8 equivalent
  }

  void update_controls(const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.update(implementation, inputs);
  }
};

}
