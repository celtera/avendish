#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/file_ports.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/binding/touchdesigner/info_output.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>

#include <TOP_CPlusPlusBase.h>

#include <vector>

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
  touchdesigner::file_ports<T> file_setup;
  TD::TOP_Context* context{};

  // Track if we need to process
  bool needs_cook{true};

  // Hold the downloaded textures alive until after the processor has run: the
  // OP_SmartRef owns the pixel buffer the avnd texture ports point into.
  std::vector<TD::OP_SmartRef<TD::OP_TOPDownloadResult>> download_results;

  // Per-input owned storage, used only when a downloaded texture must be
  // repacked (e.g. RGBA8 -> RGB8) instead of pointed at zero-copy.
  std::vector<std::vector<unsigned char>> matrix_input_scratch;

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

    download_results.clear();

    // Handle texture inputs - download from GPU to CPU
    if constexpr(avnd::matrix_input_introspection<T>::size > 0)
    {
      matrix_input_scratch.resize(avnd::matrix_input_introspection<T>::size);
      int input_index = 0;
      avnd::matrix_input_introspection<T>::for_all(
          avnd::get_inputs(implementation),
          [&]<typename Field>(Field& field) {
            if(input_index < inputs->getNumInputs())
            {
              const TD::OP_TOPInput* top_input = inputs->getInputTOP(input_index);
              if(top_input && top_input->textureDesc.width > 0 && top_input->textureDesc.height > 0)
              {
                download_matrix(top_input, field, input_index);
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
    file_setup.setup(implementation, manager);
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
  // Download texture from TD to Avendish texture port.
  // Note: the download path only handles 2D textures; OP_TOPInputDownloadOptions
  // has no texDim / slice selection so arrays / cubemaps / 3D textures cannot
  // be downloaded meaningfully here.
  template <avnd::cpu_texture_port Field>
  void download_matrix(const TD::OP_TOPInput* top_input, Field& field, int input_index)
  {
    auto& tex = field.texture;

    // Download to top-down CPU memory (avnd convention).
    TD::OP_TOPInputDownloadOptions opts;
    opts.verticalFlip = true;

    if constexpr(requires { tex.format; })
    {
      // Variable-format texture (halp::custom_variable_texture): the plug-in
      // accepts whatever TD provides. Download in the upstream's native pixel
      // format and report it back so the plug-in can interpret the channels.
      opts.pixelFormat = top_input->textureDesc.pixelFormat;

      TD::OP_SmartRef<TD::OP_TOPDownloadResult> download_result
          = top_input->downloadTexture(opts, nullptr);
      if(!download_result)
        return;

      tex.width = download_result->textureDesc.width;
      tex.height = download_result->textureDesc.height;
      tex.bytes = reinterpret_cast<decltype(tex.bytes)>(download_result->getData());
      tex.changed = true;
      map_pixel_format(download_result->textureDesc.pixelFormat, tex);

      download_results.push_back(std::move(download_result));
    }
    else
    {
      // Fixed-format (or plug-in-requested) texture: ask TD to convert from
      // whatever the upstream pixel format is into the canonical layout this
      // port expects. This makes the input robust to *any* TD source format
      // (RGBA8, BGRA8, float, mono, ...). In particular it fixes the red/blue
      // swap that happened when the upstream was BGRA8 but the bytes were read
      // as RGBA: TD now performs the channel conversion during download.
      const td_format_info fmt = get_format_info(tex);
      opts.pixelFormat = (fmt.format == TD::OP_PixelFormat::Invalid)
                             ? top_input->textureDesc.pixelFormat
                             : fmt.format;

      TD::OP_SmartRef<TD::OP_TOPDownloadResult> download_result
          = top_input->downloadTexture(opts, nullptr);
      if(!download_result)
        return;

      tex.width = download_result->textureDesc.width;
      tex.height = download_result->textureDesc.height;
      tex.changed = true;

      if(fmt.expand_rgb_to_rgba)
      {
        // TD has no 3-byte pixel format, so we requested RGBA8 and now pack it
        // down to the RGB8 layout the port expects, into per-input storage.
        const uint64_t num_pixels = static_cast<uint64_t>(tex.width) * tex.height;
        const auto* src = static_cast<const unsigned char*>(download_result->getData());
        auto& scratch = matrix_input_scratch[input_index];
        scratch.resize(num_pixels * 3);
        for(uint64_t i = 0; i < num_pixels; ++i)
        {
          scratch[i * 3 + 0] = src[i * 4 + 0];
          scratch[i * 3 + 1] = src[i * 4 + 1];
          scratch[i * 3 + 2] = src[i * 4 + 2];
        }
        tex.bytes = reinterpret_cast<decltype(tex.bytes)>(scratch.data());
        // download_result not retained: data already copied into scratch.
      }
      else
      {
        tex.bytes = reinterpret_cast<decltype(tex.bytes)>(download_result->getData());
        download_results.push_back(std::move(download_result));
      }
    }
  }

  // Upload texture from Avendish to TD
  template <avnd::cpu_texture_port Field>
  void upload_matrix(TD::TOP_Output* output, Field& field)
  {
    auto& tex = field.texture;

    // Skip if texture hasn't changed or is invalid
    if(!tex.changed || !tex.bytes || tex.width <= 0 || tex.height <= 0)
      return;

    const td_format_info fmt = get_format_info(tex);

    // Formats with no TD TOP equivalent (integer & depth formats): skip
    if(fmt.format == TD::OP_PixelFormat::Invalid)
      return;

    // Non-2D textures: producers that allocate arrays / cubemaps / 3D
    // textures carry a kind + layer count (e.g. halp::texture_kind)
    TD::OP_TexDim tex_dim = TD::OP_TexDim::e2D;
    uint32_t depth = 1;
    if constexpr(requires { tex.kind; })
    {
      using kind_type = std::decay_t<decltype(tex.kind)>;
      if constexpr(requires { kind_type::texture_array; })
        if(tex.kind == kind_type::texture_array)
          tex_dim = TD::OP_TexDim::e2DArray;
      if constexpr(requires { kind_type::cubemap; })
        if(tex.kind == kind_type::cubemap)
          tex_dim = TD::OP_TexDim::eCube;
      if constexpr(requires { kind_type::texture_3d; })
        if(tex.kind == kind_type::texture_3d)
          tex_dim = TD::OP_TexDim::e3D;

      if constexpr(requires { tex.layers_or_depth; })
        if(tex.layers_or_depth > 0)
          depth = tex.layers_or_depth;
    }

    // Calculate buffer size
    const uint64_t num_pixels = static_cast<uint64_t>(tex.width) * tex.height * depth;
    const uint64_t buffer_size = num_pixels * fmt.dst_bytes_per_pixel;

    // Create output buffer
    TD::OP_SmartRef<TD::TOP_Buffer> out_buffer =
        context->createOutputBuffer(buffer_size, TD::TOP_BufferFlags::None, nullptr);

    if(!out_buffer)
      return;

    // Copy texture data to buffer
    if(!fmt.expand_rgb_to_rgba)
    {
      std::memcpy(out_buffer->data, tex.bytes, num_pixels * fmt.src_bytes_per_pixel);
    }
    else
    {
      // TD has no 3-byte-per-pixel format: expand RGB8 to RGBA8 with an
      // opaque alpha channel
      const auto* src = reinterpret_cast<const unsigned char*>(tex.bytes);
      auto* dst = static_cast<unsigned char*>(out_buffer->data);
      for(uint64_t i = 0; i < num_pixels; ++i)
      {
        dst[i * 4 + 0] = src[i * 3 + 0];
        dst[i * 4 + 1] = src[i * 3 + 1];
        dst[i * 4 + 2] = src[i * 3 + 2];
        dst[i * 4 + 3] = 255;
      }
    }

    // Set up upload info
    TD::TOP_UploadInfo upload_info;
    upload_info.textureDesc.width = tex.width;
    upload_info.textureDesc.height = tex.height;
    upload_info.textureDesc.depth = depth;
    upload_info.textureDesc.texDim = tex_dim;
    upload_info.textureDesc.pixelFormat = fmt.format;
    upload_info.textureDesc.aspectX = tex.width;
    upload_info.textureDesc.aspectY = tex.height;
    upload_info.firstPixel = TD::TOP_FirstPixel::TopLeft;
    upload_info.colorBufferIndex = 0;

    // Upload to TouchDesigner
    output->uploadBuffer(&out_buffer, upload_info, nullptr);

    // Mark as processed
    tex.changed = false;
  }

  template <avnd::cpu_buffer_port Field>
  void download_matrix(const TD::OP_TOPInput* top_input, Field& field, int input_index)
  {
    // TODO
  }

  template <avnd::cpu_buffer_port Field>
  void upload_matrix(TD::TOP_Output* output, Field& field)
  {
    // TODO
  }

  // Pixel format mapping between the avnd CPU texture formats and TD's
  // OP_PixelFormat
  struct td_format_info
  {
    TD::OP_PixelFormat format{TD::OP_PixelFormat::Invalid};

    // Bytes per pixel on the avnd side
    int src_bytes_per_pixel{0};

    // Bytes per pixel in the TD upload buffer
    int dst_bytes_per_pixel{0};

    // TD has no 3-byte RGB pixel format: expand to RGBA8 on upload
    bool expand_rgb_to_rgba{false};
  };

  // Maps a format enumerator carried by the texture type (matched by the
  // names used in halp: RGBA8 / RGBA, BGRA8, R8, ... - see
  // halp/texture_formats.hpp) to the corresponding TD pixel format.
  // Integer formats (R8UI, R32UI, RG32UI, RGBA32UI, ...) and depth formats
  // have no TD TOP equivalent and fall through to Invalid.
  template <typename Tex, typename Fmt>
  static td_format_info map_format_value(Fmt fmt)
  {
#define AVND_TD_PIXEL_FORMAT(Name, Td, Src, Dst, Expand) \
  if constexpr(requires { Tex::Name; })                  \
    if(fmt == Tex::Name)                                 \
      return {TD::OP_PixelFormat::Td, Src, Dst, Expand};

    AVND_TD_PIXEL_FORMAT(RGBA8, RGBA8Fixed, 4, 4, false)
    AVND_TD_PIXEL_FORMAT(RGBA, RGBA8Fixed, 4, 4, false)
    AVND_TD_PIXEL_FORMAT(BGRA8, BGRA8Fixed, 4, 4, false)
    AVND_TD_PIXEL_FORMAT(R8, Mono8Fixed, 1, 1, false)
    AVND_TD_PIXEL_FORMAT(RG8, RG8Fixed, 2, 2, false)
    AVND_TD_PIXEL_FORMAT(R16, Mono16Fixed, 2, 2, false)
    AVND_TD_PIXEL_FORMAT(RG16, RG16Fixed, 4, 4, false)
    AVND_TD_PIXEL_FORMAT(RED_OR_ALPHA8, Mono8Fixed, 1, 1, false)
    AVND_TD_PIXEL_FORMAT(RGBA16F, RGBA16Float, 8, 8, false)
    AVND_TD_PIXEL_FORMAT(RGBA32F, RGBA32Float, 16, 16, false)
    AVND_TD_PIXEL_FORMAT(R16F, Mono16Float, 2, 2, false)
    AVND_TD_PIXEL_FORMAT(R32F, Mono32Float, 4, 4, false)
    AVND_TD_PIXEL_FORMAT(RGB10A2, RGB10A2Fixed, 4, 4, false)
    AVND_TD_PIXEL_FORMAT(RGB8, RGBA8Fixed, 3, 4, true)
    AVND_TD_PIXEL_FORMAT(RGB, RGBA8Fixed, 3, 4, true)
#undef AVND_TD_PIXEL_FORMAT

    return {};
  }

  // Resolve the TD pixel format of an avnd CPU texture, either from its
  // run-time format member (e.g. halp::custom_variable_texture) or from its
  // fixed format type (e.g. halp::rgba_texture)
  template <typename Tex>
  static td_format_info get_format_info(const Tex& tex)
  {
    if constexpr(requires { tex.format; })
    {
      // Run-time format chosen by the host
      return map_format_value<Tex>(tex.format);
    }
    else if constexpr(requires { tex.request_format; })
    {
      // Run-time format requested by the plug-in
      return map_format_value<Tex>(tex.request_format);
    }
    else if constexpr(requires { typename Tex::format; })
    {
      // Fixed format type: halp's fixed textures have a single enumerator
      return map_format_value<Tex>(typename Tex::format{});
    }
    else
    {
      // Default to RGBA8
      return {TD::OP_PixelFormat::RGBA8Fixed, 4, 4, false};
    }
  }

  // Map a TD pixel format back to the texture's own format enum, when the
  // texture exposes a run-time format (e.g. halp::custom_variable_texture)
  template <typename Tex>
  static void map_pixel_format(TD::OP_PixelFormat td_format, Tex& tex)
  {
#define AVND_TD_PIXEL_FORMAT(Td, Name)        \
  case TD::OP_PixelFormat::Td:                \
    if constexpr(requires { Tex::Name; })     \
    {                                         \
      tex.format = Tex::Name;                 \
      return;                                 \
    }                                         \
    break;

    switch(td_format)
    {
      AVND_TD_PIXEL_FORMAT(BGRA8Fixed, BGRA8)
      AVND_TD_PIXEL_FORMAT(RGBA8Fixed, RGBA8)
      AVND_TD_PIXEL_FORMAT(Mono8Fixed, R8)
      AVND_TD_PIXEL_FORMAT(RG8Fixed, RG8)
      AVND_TD_PIXEL_FORMAT(Mono16Fixed, R16)
      AVND_TD_PIXEL_FORMAT(RG16Fixed, RG16)
      AVND_TD_PIXEL_FORMAT(RGBA16Float, RGBA16F)
      AVND_TD_PIXEL_FORMAT(RGBA32Float, RGBA32F)
      AVND_TD_PIXEL_FORMAT(Mono16Float, R16F)
      AVND_TD_PIXEL_FORMAT(Mono32Float, R32F)
      AVND_TD_PIXEL_FORMAT(RGB10A2Fixed, RGB10A2)
      default:
        break;
    }
#undef AVND_TD_PIXEL_FORMAT

    // No matching enumerator on the avnd side: fall back to RGBA8 if possible
    if constexpr(requires { Tex::RGBA8; })
      tex.format = Tex::RGBA8;
  }

  void update_controls(const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::has_inputs<T>)
    {
      parameter_update<T>{}.update(implementation, inputs);
      file_setup.load(implementation, inputs);
    }
  }
};

}
