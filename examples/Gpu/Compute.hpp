#pragma once
#include <avnd/common/member_reflection.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <gpp/commands.hpp>
#include <gpp/meta.hpp>
#include <gpp/ports.hpp>
#include <halp/controls.hpp>
#include <halp/static_string.hpp>

#include <vector>
namespace examples
{

struct GpuComputeExample
{
  // halp_meta is a short hand for defining a static function:
  // #define halp_meta(name, val) static constexpr auto name() return { val; }
  halp_meta(name, "Average color");
  halp_meta(uuid, "03bce361-a2ca-4959-95b4-6aac3b6c07b5");
  halp_meta(category, "Visuals/Analysis")
  halp_meta(c_name, "average_color")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/"
      "computer-vision-utilities.html#average-color")
  halp_meta(description, "Extract the average color of an input video feed")

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
  const ivec2 block_size = ivec2(gl_WorkGroupSize.xy);
  ivec2 blocks = (ivec2(width, height) + block_size - 1) / block_size;
  ivec2 block = ivec2(gl_GlobalInvocationID.xy);
  if(block.x >= blocks.x || block.y >= blocks.y)
    return;

  ivec2 area = min(ivec2(width, height), imageSize(img));
  ivec2 origin = block * block_size;

  vec4 color = vec4(0.0);
  float count = 0.0;
  for(int j = 0; j < block_size.y; j++)
  {
    for(int i = 0; i < block_size.x; i++)
    {
      ivec2 p = origin + ivec2(i, j);
      if(p.x < area.x && p.y < area.y)
      {
        color += imageLoad(img, p);
        count += 1.0;
      }
    }
  }

  int index = 2 * (block.y * blocks.x + block.x);
  result[index] = color;
  result[index + 1] = vec4(count);
}
)_";
  }

  // Allocate and update buffers
  gpp::co_update update()
  {
    // Deallocate if the size changed
    const int w = blocks(this->inputs.width);
    const int h = blocks(this->inputs.height);

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
      const int bytes = w * h * 2 * sizeof(float) * 4;
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

    const int w = blocks(this->inputs.width);
    const int h = blocks(this->inputs.height);
    const int block_count = w * h;
    const int bytes = block_count * 2 * sizeof(float) * 4;

    // Run a pass
    co_yield gpp::begin_compute_pass{};

    co_yield gpp::compute_dispatch{
        .x = (w + downscale - 1) / downscale, .y = (h + downscale - 1) / downscale, .z = 1};

    // Request an asynchronous readback
    gpp::buffer_awaiter readback
        = co_yield gpp::readback_buffer{.handle = buf, .offset = 0, .size = bytes};

    co_yield gpp::end_compute_pass{};

    // The readback can be fetched once the compute pass is done
    // (this needs to be improved in terms of asyncness)
    auto [data, size] = co_yield readback;
    if(!data || size < std::size_t(bytes))
      co_return;

    using color = float[4];
    auto flt = reinterpret_cast<const color*>(data);

    // finish summing on the cpu
    auto& final = outputs.color_out.value;

    double sum[4]{};
    double count = 0.;
    for(int i = 0; i < block_count; i++)
    {
      for(int j = 0; j < 4; j++)
        sum[j] += flt[2 * i][j];
      count += flt[2 * i + 1][0];
    }

    for(int j = 0; j < 4; j++)
      final[j] = count > 0. ? float(sum[j] / count) : 0.f;
  }

private:
  static int blocks(int pixels) noexcept
  {
    return pixels > 0 ? (pixels + downscale - 1) / downscale : 0;
  }

  static constexpr auto lay = layout{};
  int last_w{}, last_h{};
  gpp::buffer_handle buf{};
  std::vector<float> zeros{};
};

}
