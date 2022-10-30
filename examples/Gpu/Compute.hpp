#pragma once
#include <avnd/common/member_reflection.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <gpp/commands.hpp>
#include <gpp/meta.hpp>
#include <gpp/ports.hpp>
#include <halp/controls.hpp>
#include <halp/static_string.hpp>

namespace examples
{

struct GpuComputeExample
{
  // halp_meta is a short hand for defining a static function:
  // #define halp_meta(name, val) static constexpr auto name() return { val; }
  halp_meta(name, "Compute pipeline");
  halp_meta(uuid, "03bce361-a2ca-4959-95b4-6aac3b6c07b5");

  static constexpr int downscale = 16;

  // Define the layout of our pipeline in C++ simply through the structure of a struct
  struct layout
  {
    halp_meta(local_size_x, 16)
    halp_meta(local_size_y, 16)
    halp_meta(local_size_z, 1)
    halp_flags(compute);

    struct bindings
    {
      // Each binding is a struct member
      struct
      {
        halp_meta(name, "my_buf");
        halp_meta(binding, 0);
        halp_flags(std140, buffer, load, store);

        using color = float[4];
        gpp::uniform<"result", color*> values;
      } my_buf;

      // Define the members of our ubos
      struct custom_ubo
      {
        halp_meta(name, "custom");
        halp_meta(binding, 1);
        halp_flags(std140, ubo);

        gpp::uniform<"width", int> width;
        gpp::uniform<"height", int> height;
      } ubo;

      struct
      {
        halp_meta(name, "img")
        halp_meta(format, "rgba32f")
        halp_meta(binding, 2);
        halp_flags(image2D, readonly);
      } image;
    } bindings;
  };

  using bindings = decltype(layout::bindings);
  using uniforms = decltype(bindings::ubo);

  // Definition of our ports which will get parsed by the
  // software that instantiate this class
  struct
  {
    // Here we use some helper types in the usual fashion
    gpp::image_input_port<"Image", &bindings::image> tex;

    gpp::uniform_control_port<
        halp::hslider_i32<"Width", halp::range{1, 1000, 100}>, &uniforms::width>
        width;

    gpp::uniform_control_port<
        halp::hslider_i32<"Height", halp::range{1, 1000, 100}>, &uniforms::height>
        height;
  } inputs;

  // The output port on which we write the average color
  struct
  {
    struct
    {
      halp_meta(name, "color")
      float value[4];
    } color_out;
  } outputs;

  std::string_view compute()
  {
    return R"_(
void main()
{
  // Note: the algorithm is most likely wrong as I know FUCK ALL
  // about compute shaders ; fixes welcome ;p

  ivec2 call = ivec2(gl_GlobalInvocationID.xy);
  vec4 color = vec4(0.0, 0.0,0,0);

  for(int i = 0; i < gl_WorkGroupSize.x; i++)
  {
    for(int j = 0; j < gl_WorkGroupSize.y; j++)
    {
      uint x = call.x * gl_WorkGroupSize.x + i;
      uint y = call.y * gl_WorkGroupSize.y + j;

      if (x < width && y < height)
      {
        color += imageLoad(img, ivec2(x,y));
      }
    }
  }

  if(gl_LocalInvocationIndex < ((width * height) / gl_WorkGroupSize.x * gl_WorkGroupSize.y))
  {
    result[gl_GlobalInvocationID.y * gl_WorkGroupSize.x + gl_GlobalInvocationID.x] = color;
  }
}
)_";
  }

  // Allocate and update buffers
  gpp::co_update update()
  {
    // Deallocate if the size changed
    const int w = this->inputs.width / downscale;
    const int h = this->inputs.height / downscale;
    if(last_w != w || last_h != h)
    {
      if(this->buf)
      {
        co_yield gpp::buffer_release{.handle = buf};
        buf = nullptr;
      }
      last_w = w;
      last_h = h;
    }

    if(w > 0 && h > 0)
    {
      // No buffer: reallocate
      const int bytes = w * h * sizeof(float) * 4;
      if(!this->buf)
      {
        this->buf = co_yield gpp::static_allocation{
            .binding = lay.bindings.my_buf.binding(), .size = bytes};
      }
    }
  }

  // Relaease allocated data
  gpp::co_release release()
  {
    if(buf)
    {
      co_yield gpp::buffer_release{.handle = buf};
      buf = nullptr;
    }
  }

  // Do the GPU dispatch call
  gpp::co_dispatch dispatch()
  {
    if(!buf)
      co_return;

    const int w = this->inputs.width / downscale;
    const int h = this->inputs.height / downscale;
    const int downscaled_pixels_count = w * h;
    const int bytes = downscaled_pixels_count * sizeof(float) * 4;

    // Run a pass
    co_yield gpp::begin_compute_pass{};

    co_yield gpp::compute_dispatch{.x = 1, .y = 1, .z = 1};

    // Request an asynchronous readback
    gpp::buffer_awaiter readback
        = co_yield gpp::readback_buffer{.handle = buf, .offset = 0, .size = bytes};

    co_yield gpp::end_compute_pass{};

    // The readback can be fetched once the compute pass is done
    // (this needs to be improved in terms of asyncness)
    auto [data, size] = co_yield readback;

    using color = float[4];
    auto flt = reinterpret_cast<const color*>(data);

    // finish summing on the cpu
    auto& final = outputs.color_out.value;

    final[0] = 0.f;
    final[1] = 0.f;
    final[2] = 0.f;
    final[3] = 0.f;

    for(int i = 0; i < downscaled_pixels_count; i++)
    {
      for(int j = 0; j < 4; j++)
      {
        final[j] += flt[i][j];
      }
    }

    final[0] /= downscaled_pixels_count;
    final[1] /= downscaled_pixels_count;
    final[2] /= downscaled_pixels_count;
    final[3] /= downscaled_pixels_count;
  }

private:
  static constexpr auto lay = layout{};
  int last_w{}, last_h{};
  gpp::buffer_handle buf{};
  std::vector<float> zeros{};
};

}
