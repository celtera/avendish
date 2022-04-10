#pragma once
#include <gpp/meta.hpp>
#include <gpp/ports.hpp>
#include <gpp/commands.hpp>
#include <gpp/layout.hpp>

namespace examples
{
struct GpuRawExample
{
  halp_meta(name, "Raw GPU pipeline");
  halp_meta(uuid, "63612002-6b83-4b23-9844-9ec59badf1d5");

  // Define the layout of our pipeline in C++ simply thorugh the structure of a struct
  static constexpr struct layout
  {
    // We state that this layout is for a graphics pipeline
    enum { graphics };

    // Define the vertex inputs
    struct vertex_input
    {
      // Of course in practice we'd use some custom types or macros here,
      // this is just to show that there's no magic or special type involved
      struct {
        halp_meta(name, "v_position");
        halp_flag(position);
        static constexpr int location() { return 0; }
        float data[3];
      } vertex;

      struct {
        halp_meta(name, "v_texcoord");
        halp_flag(texcoord);
        static constexpr int location() { return 1; }
        float data[2];
      } texcoord;
    } vertex_input;

    // Define the vertex outputs
    struct vertex_output {
      struct {
        halp_meta(name, "texcoord");
        static constexpr int location() { return 0; }
        float data[2];
      } texcoord;

      struct {
        halp_meta(name, "gl_Position");
        enum { per_vertex };
        float data[4];
      } position;
    } vertex_output;

    // Define the fragment inputs
    struct fragment_input {
      struct {
        halp_meta(name, "texcoord");
        static constexpr int location() { return 0; }
        float data[2];
      } texcoord;
    } fragment_input;

    // Define the fragment outputs
    struct fragment_output {
      struct {
        halp_meta(name, "fragColor");
        static constexpr int location() { return 0; }
        float data[4];
      } fragColor;
    } fragment_output;


    // Define the ubos, samplers, etc.
    struct bindings
    {
      struct custom_ubo {
        halp_meta(name, "custom");
        enum { std140 };
        enum { ubo };

        static constexpr int binding() { return 0; }
        struct
        {
          halp_meta(name, "Foo");
          float value[2];
        } pad;

        struct
        {
          halp_meta(name, "Bar");
          float value;
        } slider;
      } ubo;

      struct sampler
      {
        halp_meta(name, "tex");
        enum { sampler2D };
        static constexpr int binding() { return 1; }
      } texture_input;
    } bindings;
  } lay{};

  using bindings = decltype(layout::bindings);

  struct {
    // If samplers & buffers are referenced here they will be automatically allocated
    // as they are expected to come from "outside"

    // Otherwise it is the responsibility of the author to allocate
    // them in update();
  } inputs;

  struct {
    struct {
      halp_meta(name, "Color output");
      static constexpr auto attachment() { return &layout::fragment_output; }
    } color_att;
  } outputs;

  std::string_view vertex()
  {
    return R"_(
void main()
{
  texcoord = v_texcoord;
  gl_Position = vec4(v_position.x / 3., v_position.y / 3, 0.0, 1.);
}
)_";
  }

  std::string_view fragment()
  {
    return R"_(
void main()
{
  fragColor = vec4(texture(tex, texcoord.xy).rgb, 1.0) ;
}
)_";
  }

  gpp::co_update update()
  {
    constexpr int ubo_size = gpp::std140_size<bindings::custom_ubo>();

    if (!buf_handle)
    {
      // Resize our local, CPU-side buffer
      buf.resize(ubo_size);

      // Request the creation of a GPU buffer
      this->buf_handle = co_yield gpp::dynamic_ubo_allocation{
          .binding = lay.bindings.ubo.binding()
        , .size = ubo_size
      };
    }

    // Upload some data into it
    co_yield gpp::dynamic_ubo_upload{
        .handle = buf_handle,
        .offset = 0,
        .size = ubo_size,
        .data = buf.data()
    };

    // Same for the texture
    int sz = 16*16*4;
    if(!tex_handle)
    {
      this->tex_handle = co_yield gpp::texture_allocation{
          .binding = lay.bindings.texture_input.binding()
        , .width = 16
        , .height = 16
      };
    }

    tex.resize(sz);
    for(int i = 0; i < sz; i++)
      tex[i] = rand();

    co_yield gpp::texture_upload{
        .handle = tex_handle
      , .offset = 0
      , .size = sz
      , .data = tex.data()
    };
  }
private:
  std::vector<float> buf;
  std::vector<uint8_t> tex;

  gpp::buffer_handle buf_handle{};
  gpp::texture_handle tex_handle{};

};

}
