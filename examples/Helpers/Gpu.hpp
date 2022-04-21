#pragma once

#include <boost/pfr/core.hpp>
// #include <halp/meta.hpp>
// #include <halp/controls.hpp>

// #include <avnd/common/coroutines.hpp>
#include <variant>
#include <coroutine>

#define halp_meta(name, value) \
  static consteval auto name() { return value; }


/*
#include <halp/audio.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <halp/texture.hpp>
#include <cmath>
*/
namespace gpu
{
enum class layouts { std140 };
enum class samplers { sampler1D, sampler2D, sampler3D, samplerCube, sampler2DRect };
enum class buffer_type { immutable_buffer, static_buffer, dynamic_buffer };
enum class buffer_usage { vertex, index, uniform, storage };
enum class binding_stage { vertex, fragment };

enum class default_attributes {
  position, normal, tangent, texcoord, color
};

enum class default_uniforms {
    // Render-level default uniforms
      clip_space_matrix // float[16]
    , texcoord_adjust // float[2]
    , render_size // float[2]

    // Process-level default uniforms
    , time // float
    , time_delta // float
    , progress // float
    , pass_index // int
    , frame_index // int
    , date // float[4]
    , mouse // float[4]
    , channel_time // float[4]
    , sample_rate // float
};

template<typename T>
consteval int std140_size()
{
    int sz = 0;
    constexpr int field_count = pfr::tuple_size_v<T>;
    auto func = [&] (auto field) {
        switch(sizeof(field.value)) {
        case 4:
            sz += 4;
            break;
        case 8:
            if(sz % 8 != 0)
                sz +=4;
            sz += 8;
            break;
        case 12:
            while(sz % 16 != 0)
                sz += 4;
            sz += 12;
            break;
        case 16:
            while(sz % 16 != 0)
                sz += 4;
            sz += 16;
            break;
        }
    };

    if constexpr (field_count > 0)
    {
      [&func]<typename K, K... Index>(std::integer_sequence<K, Index...>)
      {
        constexpr T t;
        (func(pfr::get<Index, T>(t)), ...);
      }(std::make_index_sequence<field_count>{});
    }
    return sz;
}

template <typename Out, typename In>
class generator
{
public:
  // Types used by the coroutine
  struct promise_type
  {
    Out current_command;
    In feedback_value;

    struct awaiter : std::suspend_always {
      friend promise_type;
      constexpr In await_resume() const { return p.feedback_value; }

      promise_type& p;
    };

    generator get_return_object() { return generator{handle::from_promise(*this)}; }

    static std::suspend_always initial_suspend() noexcept { return {}; }

    static std::suspend_always final_suspend() noexcept { return {}; }

    awaiter yield_value(Out value) noexcept
    {
      current_value = value;
      return awaiter{{}, *this};
    }

    void return_void() noexcept { }

    // Disallow co_await in generator coroutines.
    void await_transform() = delete;

    [[noreturn]] static void unhandled_exception() { std::abort(); }
  };

  using handle = std::coroutine_handle<promise_type>;

  // To enable begin / end
  class iterator
  {
  public:
    explicit iterator(const handle& coroutine) noexcept
        : m_coroutine{coroutine}
    {
    }

    void operator++() noexcept { m_coroutine.resume(); }

    Out& operator*() const noexcept { return m_coroutine.promise(); }

    bool operator==(std::default_sentinel_t) const noexcept
    {
      return !m_coroutine || m_coroutine.done();
    }

  private:
    handle m_coroutine;
  };

  // Constructors
  explicit generator(handle coroutine)
      : m_coroutine{std::move(coroutine)}
  {
  }

  generator() noexcept = default;
  generator(const generator&) = delete;
  generator& operator=(const generator&) = delete;

  generator(generator&& other) noexcept
      : m_coroutine{other.m_coroutine}
  {
    other.m_coroutine = {};
  }

  generator& operator=(generator&& other) noexcept
  {
    if (this != &other)
    {
      if (m_coroutine)
      {
        m_coroutine.destroy();
      }

      m_coroutine = other.m_coroutine;
      other.m_coroutine = {};
    }
    return *this;
  }

  ~generator()
  {
    if (m_coroutine)
    {
      m_coroutine.destroy();
    }
  }

  // Range-based for loop support.
  iterator begin() noexcept
  {
    if (m_coroutine)
    {
      m_coroutine.resume();
    }

    return iterator{m_coroutine};
  }

  std::default_sentinel_t end() const noexcept { return {}; }

private:
  handle m_coroutine;
};
}

namespace examples
{
struct layout
{
    struct {
        struct {
            static constexpr int location() { return 0; }
            halp_meta(default_attribute, gpu::default_attributes::position);
            float data[3];
        } vertex;

        struct {
            static constexpr int location() { return 1; }
            halp_meta(default_attribute, gpu::default_attributes::normal);
            float data[2];
        } normal;

        struct {
            static constexpr int location() { return 2; }
            halp_meta(default_attribute, gpu::default_attributes::texcoord);
            float data[2];
        } texcoord;
    } input;

    struct {
        struct {
            static constexpr int location() { return 0; }
            halp_meta(default_attribute, gpu::default_attributes::color);
            float data[4];
        } fragColor;
    } output;

    struct {
        struct {
            halp_meta(name, "renderer");
            halp_meta(layout, gpu::layouts::std140);
            static constexpr int binding() { return 0; }
            struct {
                halp_meta(name, "clipSpaceCorrMatrix");
                halp_meta(default_uniform, gpu::default_uniforms::clip_space_matrix);
            } clip_space_matrix;
            struct {
                halp_meta(name, "texcoordAdjust");
                halp_meta(default_uniform, gpu::default_uniforms::texcoord_adjust);
            } texcoord_adjust;
            struct {
                halp_meta(name, "renderSize");
                halp_meta(default_uniform, gpu::default_uniforms::render_size);
            } render_size;
        } renderer;

        struct {
            halp_meta(name, "process");
            halp_meta(layout, gpu::layouts::std140);
            static constexpr int binding() { return 1; }
            struct {
                halp_meta(name, "time");
                halp_meta(default_uniform, gpu::default_uniforms::time);
            } time;
            struct {
                halp_meta(name, "timeDelta");
                halp_meta(default_uniform, gpu::default_uniforms::time_delta);
            } time_delta;
            struct {
                halp_meta(name, "progress");
                halp_meta(default_uniform, gpu::default_uniforms::progress);
            } progress;

            struct {
                halp_meta(name, "pass_index");
                halp_meta(default_uniform, gpu::default_uniforms::pass_index);
            } pass_index;
            struct {
                halp_meta(name, "frame_index");
                halp_meta(default_uniform, gpu::default_uniforms::frame_index);
            } frame_index;

            struct {
                halp_meta(name, "date");
                halp_meta(default_uniform, gpu::default_uniforms::date);
            } date;
            struct {
                halp_meta(name, "mouse");
                halp_meta(default_uniform, gpu::default_uniforms::mouse);
            } mouse;
            struct {
                halp_meta(name, "channelTime");
                halp_meta(default_uniform, gpu::default_uniforms::channel_time);
            } channel_time;

            struct {
                halp_meta(name, "sampleRate");
                halp_meta(default_uniform, gpu::default_uniforms::sample_rate);
            } sample_rate;
        } ubo_0;

        struct {
            halp_meta(name, "custom");
            halp_meta(layout, gpu::layouts::std140);
            static constexpr int binding() { return 2; }
            halp::xy_pad_f32<"Foo"> foo;
            halp::hslider_f32<"Bar"> bar;
        } custom_ubo;

        struct {
            halp_meta(name, "texture");
            halp_meta(format, gpu::samplers::sampler2D);
            static constexpr int binding() { return 3; }
        } texture_input;
    } bindings;
};

struct gpu_texture_input
{
    int handle;
    // ? int width;
    // ? int height;
    // ? int format;
};

struct gpu_mesh_input
{
};

struct gpu_buffer_input
{
    int handle;
    int size;
};

struct gpu_texture_output
{
    int handle;
    int width;
    int height;
    int format;
};

struct gpu_buffer_output
{
    int handle;
    int size;
};

struct GpuFilterExample
{
    halp_meta(name, "My GPU texture filter");
    halp_meta(c_name, "gpu_filt");
    halp_meta(category, "Demo");
    halp_meta(author, "Jean-Michaël Celerier");
    halp_meta(description, "Example GPU filter");
    halp_meta(uuid, "542f7838-0f3f-4301-b158-f516f51d4427");

    struct {
        struct {
            halp_meta(name, "In")
            gpu_mesh_input mesh;
        } mesh;
        struct {
            halp_meta(name, "In")
            gpu_texture_input texture;
        } image;
        /*
      struct {
          halp_meta(name, "In buf")
          gpu_buffer_input buffer;
      } buffer;
*/
        halp::hslider_f32<"Control"> control;
    } inputs;

    struct
    {
        struct {
            halp_meta(name, "Out")
            gpu_texture_output texture;
        } image;
    } outputs;
    /*
  std::string_view vertex()
  {
      return "";
  }
*/
    std::string_view fragment()
    {
        return R"_(#version 450

layout(location = 0) in vec3 esVertex;
layout(location = 1) in vec3 esNormal;
layout(location = 2) in vec2 v_texcoord;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform renderer {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  vec2 renderSize;
};

layout(std140, binding = 1) uniform process {
  float time;
  float timeDelta;
  float progress;

  int passIndex;
  int frameIndex;

  vec4 date;
  vec4 mouse;
  vec4 channelTime;

  float sampleRate;
};

layout(std140, binding = 2) uniform custom {
  vec2 foo;
  float bar;
};

layout(binding = 3) uniform sampler2D texture;

void main()
{
  fragColor = vec4(1,1,1,1);
}

)_";
    }
    /*
  std::string_view compute()
  {
      return "";
  }
*/

    static constexpr examples::layout layout{};

    struct buffer_allocation_action {
        enum { dynamic_buffer_allocation };
        int binding;
        int size;
    };
    using buffer_allocation = buffer_allocation_action;

    struct buffer_upload_action {
        enum { dynamic_buffer_upload };
        void* handle;
        int offset;
        int size;
        void* data;
    };
    using buffer_upload = buffer_upload_action;

    struct texture_upload_action {
        enum { texture_upload };
        void* handle;
        int offset;
        int size;
        void* data;
    };
    using texture_upload = texture_upload_action;

    using action = std::variant<
      buffer_allocation
    , buffer_upload
    , texture_upload
    >;
    std::vector<float> buf;

    void* buf_handle{};

    gpu::generator<action, void*> update()
    {
      constexpr int ubo_size = gpu::std140_size(layout.bindings.custom_ubo);
      if(!buf_handle)
      {
        // Resize our local, CPU-side buffer
        buf.resize(ubo_size);

        // Request the creation of a GPU buffer
        this->buf_handle = co_yield buffer_allocation{
            .binding = layout.bindings.custom_ubo.binding()
          , .size = ubo_size
        };
      }

      // Upload some data into it
      co_yield buffer_upload{
            .handle = buf_handle
          , .offset = 0
          , .size = ubo_size
          , .data = buf.data()
       };
    }
/*
    void draw()
    {

    }
*/
};

enum commands {
    static_buffer_allocation,
    static_buffer_upload,
    dynamic_buffer_allocation,
    dynamic_buffer_upload,
    immutable_buffer_allocation,
    immutable_buffer_upload,
    texture_allocation,
    texture_upload

};

struct command_indices {
    int buffer_allocation{-1};
    int buffer_upload;
    int texture_upload;
};
template<typename... Args>
static constexpr auto command_indices(std::variant<Args...>&&) {
}

struct handle_command {
  template<typename C>
  void* operator()(C& command)
  {
    if constexpr(requires { C::static_buffer_allocation; }) {
        return nullptr;
    }
    else if constexpr(requires { C::dynamic_buffer_allocation; }) {
        return nullptr;
    }
    else if constexpr(requires { C::immutable_buffer_allocation; }) {
        return nullptr;
    }
    else if constexpr(requires { C::texture_allocation; }) {
        return nullptr;
    }
    else if constexpr(requires { C::static_buffer_upload; }) {
        return nullptr;
    }
    else if constexpr(requires { C::dynamic_buffer_upload; }) {
        return nullptr;
    }
    else if constexpr(requires { C::immutable_buffer_upload; }) {
        return nullptr;
    }
    else if constexpr(requires { C::texture_upload; }) {
        return nullptr;
    }
  }
};

template<typename T>
void handle_update(T& object)
{
    for(auto& promise : object.update()) {
        promise.feedback_value = std::visit(handle_command{}, promise.current_command);

    }
}

int main()
{

}
}
