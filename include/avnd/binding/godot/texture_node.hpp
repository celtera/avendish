#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/godot/node.hpp>
#include <avnd/introspection/port.hpp>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <cstring>

namespace godot_binding
{

/// Map an Avendish texture format tag (compile-time) to a Godot Image::Format.
///
/// Handles:
/// - textures whose format is a static string, e.g. static constexpr auto format() { return "rgba"; }
/// - textures whose format is a one-value enum tag, e.g. enum format { RGBA };
///   (this is what halp::rgba_texture, rgb_texture, r8_texture, r32f_texture,
///    rgba32f_texture use)
/// - anything else falls back to a guess based on bytes_per_pixel, else RGBA8.
template <typename F>
constexpr godot::Image::Format godot_image_format() noexcept
{
  if constexpr(requires { std::string_view{F::format()}; })
  {
    constexpr std::string_view fmt = F::format();

    if(fmt == "rgba" || fmt == "rgba8")
      return godot::Image::FORMAT_RGBA8;
    else if(fmt == "r8")
      return godot::Image::FORMAT_R8;
    else if(fmt == "grayscale" || fmt == "l8")
      return godot::Image::FORMAT_L8;
    else if(fmt == "rg8")
      return godot::Image::FORMAT_RG8;
    else if(fmt == "rgba16f")
      return godot::Image::FORMAT_RGBAH;
    else if(fmt == "rgba32f")
      return godot::Image::FORMAT_RGBAF;
    else if(fmt == "r16f")
      return godot::Image::FORMAT_RH;
    else if(fmt == "r32f")
      return godot::Image::FORMAT_RF;
    else if(fmt == "rg16f")
      return godot::Image::FORMAT_RGH;
    else if(fmt == "rg32f")
      return godot::Image::FORMAT_RGF;
    else if(fmt == "rgb32f")
      return godot::Image::FORMAT_RGBF;
    else if(fmt == "rgb" || fmt == "rgb8")
      return godot::Image::FORMAT_RGB8;
    else
      return godot::Image::FORMAT_RGBA8;
  }
  else if constexpr(requires { requires std::is_enum_v<typename F::format>; })
  {
    if constexpr(requires { F::RGBA; } || requires { F::RGBA8; })
      return godot::Image::FORMAT_RGBA8;
    else if constexpr(requires { F::R8; })
      return godot::Image::FORMAT_R8;
    else if constexpr(requires { F::GRAYSCALE; } || requires { F::L8; })
      return godot::Image::FORMAT_L8;
    else if constexpr(requires { F::RG8; })
      return godot::Image::FORMAT_RG8;
    else if constexpr(requires { F::RGBA16F; })
      return godot::Image::FORMAT_RGBAH;
    else if constexpr(requires { F::RGBA32F; })
      return godot::Image::FORMAT_RGBAF;
    else if constexpr(requires { F::R16F; })
      return godot::Image::FORMAT_RH;
    else if constexpr(requires { F::R32F; })
      return godot::Image::FORMAT_RF;
    else if constexpr(requires { F::RG16F; })
      return godot::Image::FORMAT_RGH;
    else if constexpr(requires { F::RG32F; })
      return godot::Image::FORMAT_RGF;
    else if constexpr(requires { F::RGB32F; })
      return godot::Image::FORMAT_RGBF;
    else if constexpr(requires { F::RGB; } || requires { F::RGB8; })
      return godot::Image::FORMAT_RGB8;
    else
      return godot::Image::FORMAT_RGBA8;
  }
  else if constexpr(requires { std::size_t{F::bytes_per_pixel}; })
  {
    // No format information; guess from the static pixel stride.
    if constexpr(F::bytes_per_pixel == 1)
      return godot::Image::FORMAT_L8;
    else if constexpr(F::bytes_per_pixel == 2)
      return godot::Image::FORMAT_RG8;
    else if constexpr(F::bytes_per_pixel == 3)
      return godot::Image::FORMAT_RGB8;
    else
      return godot::Image::FORMAT_RGBA8;
  }
  else
  {
    return godot::Image::FORMAT_RGBA8;
  }
}

/// Map a run-time texture format enum value (halp::custom_texture_base::texture_format
/// style) to a Godot Image::Format. Unsupported formats fall back to RGBA8.
template <typename E>
godot::Image::Format godot_image_format_runtime(E fmt) noexcept
{
#define AVND_GODOT_MAP_FORMAT(av, gd)        \
  if constexpr(requires { E::av; })          \
    if(fmt == E::av)                         \
      return godot::Image::gd;
  AVND_GODOT_MAP_FORMAT(RGBA8, FORMAT_RGBA8)
  // Godot has no BGRA8 CPU image format; same stride, swapped channels.
  AVND_GODOT_MAP_FORMAT(BGRA8, FORMAT_RGBA8)
  AVND_GODOT_MAP_FORMAT(R8, FORMAT_R8)
  AVND_GODOT_MAP_FORMAT(RG8, FORMAT_RG8)
  AVND_GODOT_MAP_FORMAT(RED_OR_ALPHA8, FORMAT_L8)
  AVND_GODOT_MAP_FORMAT(RGBA16F, FORMAT_RGBAH)
  AVND_GODOT_MAP_FORMAT(RGBA32F, FORMAT_RGBAF)
  AVND_GODOT_MAP_FORMAT(R16F, FORMAT_RH)
  AVND_GODOT_MAP_FORMAT(R32F, FORMAT_RF)
  AVND_GODOT_MAP_FORMAT(RGB8, FORMAT_RGB8)
#undef AVND_GODOT_MAP_FORMAT
  return godot::Image::FORMAT_RGBA8;
}

/// Format of a concrete texture instance.
/// Dispatches to the run-time mapping for dynamic-format textures
/// (halp::custom_texture / halp::custom_variable_texture), and to the
/// compile-time mapping for fixed-format ones.
template <typename Tex>
godot::Image::Format godot_image_format_of(const Tex& tex) noexcept
{
  if constexpr(requires {
                 requires std::is_enum_v<
                     std::remove_cvref_t<decltype(Tex::request_format)>>;
               })
    return godot_image_format_runtime(tex.request_format);
  else if constexpr(requires {
                      requires std::is_enum_v<
                          std::remove_cvref_t<decltype(Tex::format)>>;
                    })
    return godot_image_format_runtime(tex.format);
  else
    return godot_image_format<Tex>();
}

/// Size in bytes of the pixel data of a texture instance.
template <typename Tex>
int godot_texture_bytesize(const Tex& tex) noexcept
{
  if constexpr(requires { int(tex.bytesize()); })
    return tex.bytesize();
  else if constexpr(requires { std::size_t{Tex::bytes_per_pixel}; })
    return tex.width * tex.height * Tex::bytes_per_pixel;
  else if constexpr(requires { int(tex.bytes_per_pixel()); })
    return tex.width * tex.height * tex.bytes_per_pixel();
  else
    return tex.width * tex.height * 4;
}

/**
 * Godot Node wrapping an Avendish texture generator/filter.
 *
 * Each frame, runs the processor's operator() and uploads output textures
 * to Godot ImageTexture resources. The textures are exposed as properties
 * so they can be assigned to Sprite2D, TextureRect, etc.
 *
 * CPU texture inputs are exposed as settable Texture2D properties: when one
 * is assigned, its image is read back, converted to the format expected by
 * the port (halp::rgba_texture, rgb_texture, r8_texture, r32f_texture,
 * rgba32f_texture, ...) and handed to the processor.
 *
 * Note: only 2D textures are supported. halp::texture_kind
 * (texture_array / cubemap / texture_3d, declared through texture_target())
 * only exists on GPU texture ports for now; CPU-side halp textures carry no
 * layer/depth data, so there is nothing to feed Godot's Texture2DArray /
 * Cubemap / ImageTexture3D with. See the report in the binding docs.
 */
template <typename T>
struct godot_texture_node : public godot::Node
{
  mutable avnd::effect_container<T> effect;

  static constexpr int tex_output_count
      = avnd::cpu_texture_output_introspection<T>::size;
  static constexpr int tex_input_count
      = avnd::cpu_texture_input_introspection<T>::size;

  godot::Ref<godot::ImageTexture> output_textures[tex_output_count > 0
                                                       ? tex_output_count
                                                       : 1];

  // The Texture2D resources assigned by the user, and the CPU-side pixel
  // storage (converted to the port's format) that the processor reads from.
  godot::Ref<godot::Texture2D>
      input_textures[tex_input_count > 0 ? tex_input_count : 1];
  godot::PackedByteArray
      input_buffers[tex_input_count > 0 ? tex_input_count : 1];

  godot_texture_node()
  {
    if constexpr(avnd::has_inputs<T>)
    {
      for(auto state : effect.full_state())
      {
        avnd::for_each_field_ref(state.inputs, [&]<typename Ctl>(Ctl& ctl) {
          if constexpr(avnd::has_range<Ctl>)
          {
            static_constexpr auto c = avnd::get_range<Ctl>();
            if_possible(ctl.value = c.values[c.init].second)
                else if_possible(ctl.value = c.values[c.init])
                else if_possible(ctl.value = c.init);
          }
        });
      }

      // CPU texture input ports are plain views: make sure the processor
      // sees an empty texture instead of indeterminate garbage until one
      // is assigned.
      if constexpr(tex_input_count > 0)
      {
        avnd::cpu_texture_input_introspection<T>::for_all(
            [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
          auto& port = avnd::pfr::get<Idx>(effect.inputs());
          port.texture.bytes = nullptr;
          port.texture.width = 0;
          port.texture.height = 0;
          port.texture.changed = false;
        });
      }
    }

    for(int i = 0; i < tex_output_count; ++i)
    {
      output_textures[i].instantiate();
    }
  }

  ~godot_texture_node() override = default;

  void do_ready()
  {
    if constexpr(avnd::has_inputs<T>)
    {
      for(auto state : effect.full_state())
      {
        avnd::for_each_field_ref(state.inputs, [&]<typename Ctl>(Ctl& ctl) {
          if_possible(ctl.update(state.effect));
        });
      }
    }
  }

  void do_process(double delta)
  {
    // Run the processor
    if constexpr(requires { effect.effect({delta}); })
      effect.effect(delta);
    else if constexpr(requires { effect.effect(); })
      effect.effect();

    // Upload output textures to Godot
    upload_textures();
  }

  /// Get the Nth output texture as a Godot ImageTexture
  godot::Ref<godot::ImageTexture> get_output_texture(int idx) const
  {
    if(idx >= 0 && idx < tex_output_count)
      return output_textures[idx];
    return {};
  }

  void do_get_property_list(godot::List<godot::PropertyInfo>* p_list) const
  {
    // Expose parameter properties
    godot_binding::get_property_list<T>(p_list);

    // Expose input textures as assignable Texture2D properties
    if constexpr(tex_input_count > 0)
    {
      avnd::cpu_texture_input_introspection<T>::for_all(
          [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
        using inputs_t = typename avnd::inputs_type<T>::type;
        auto tex_name
            = godot::StringName(field_name<Idx, C, inputs_t>().data());
        p_list->push_back(godot::PropertyInfo(
            godot::Variant::OBJECT, tex_name,
            godot::PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"));
      });
    }

    // Expose output textures as read-only properties
    avnd::cpu_texture_output_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      using outputs_t = typename avnd::outputs_type<T>::type;
      auto tex_name
          = godot::StringName(field_name<Idx, C, outputs_t>().data());
      p_list->push_back(godot::PropertyInfo(
          godot::Variant::OBJECT, tex_name, godot::PROPERTY_HINT_RESOURCE_TYPE,
          "ImageTexture"));
    });
  }

  bool do_set(const godot::StringName& p_name, const godot::Variant& p_value)
  {
    // Check input textures first
    if constexpr(tex_input_count > 0)
    {
      int tex_idx = 0;
      bool found = false;
      avnd::cpu_texture_input_introspection<T>::for_all(
          [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
        if(found)
          return;
        using inputs_t = typename avnd::inputs_type<T>::type;
        static const godot::StringName tex_name{
            field_name<Idx, C, inputs_t>().data()};
        if(tex_name == p_name)
        {
          set_input_texture<Idx>(tex_idx, p_value);
          found = true;
        }
        ++tex_idx;
      });
      if(found)
        return true;
    }

    return godot_binding::set_property<T>(effect, p_name, p_value);
  }

  bool do_get(const godot::StringName& p_name, godot::Variant& r_ret) const
  {
    // Check input textures first
    if constexpr(tex_input_count > 0)
    {
      int tex_idx = 0;
      bool found = false;
      avnd::cpu_texture_input_introspection<T>::for_all(
          [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
        if(found)
          return;
        using inputs_t = typename avnd::inputs_type<T>::type;
        static const godot::StringName tex_name{
            field_name<Idx, C, inputs_t>().data()};
        if(tex_name == p_name)
        {
          r_ret = input_textures[tex_idx];
          found = true;
        }
        ++tex_idx;
      });
      if(found)
        return true;
    }

    // Then output textures
    int tex_idx = 0;
    bool found = false;
    avnd::cpu_texture_output_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      if(found)
        return;
      using outputs_t = typename avnd::outputs_type<T>::type;
      static const godot::StringName tex_name{
          field_name<Idx, C, outputs_t>().data()};
      if(tex_name == p_name)
      {
        r_ret = output_textures[tex_idx];
        found = true;
      }
      ++tex_idx;
    });
    if(found)
      return true;

    return godot_binding::get_property<T>(effect, p_name, r_ret);
  }

  bool do_property_can_revert(const godot::StringName& p_name) const
  {
    return godot_binding::property_can_revert<T>(p_name);
  }

  bool do_property_get_revert(
      const godot::StringName& p_name, godot::Variant& r_property) const
  {
    return godot_binding::property_get_revert<T>(p_name, r_property);
  }

private:
  /// Assign a Godot Texture2D to the FieldIdx-th input port (which is the
  /// tex_idx-th CPU texture input). The image is read back to the CPU,
  /// converted to the format expected by the port, and kept alive in
  /// input_buffers[tex_idx] until the next assignment.
  template <auto FieldIdx>
  void set_input_texture(int tex_idx, const godot::Variant& p_value)
  {
    auto& port = avnd::pfr::get<FieldIdx>(effect.inputs());

    godot::Ref<godot::Texture2D> tex = p_value;
    input_textures[tex_idx] = tex;

    if(tex.is_null())
    {
      input_buffers[tex_idx] = godot::PackedByteArray{};
      port.texture.bytes = nullptr;
      port.texture.width = 0;
      port.texture.height = 0;
      port.texture.changed = true;
      return;
    }

    godot::Ref<godot::Image> img = tex->get_image();
    if(img.is_null() || img->is_empty())
      return;

    // get_image() returns a copy, so we can convert in-place.
    if(img->is_compressed())
    {
      if(img->decompress() != godot::OK)
        return;
    }
    if(img->has_mipmaps())
      img->clear_mipmaps();

    const auto fmt = godot_image_format_of(port.texture);
    if(img->get_format() != fmt)
      img->convert(fmt);

    input_buffers[tex_idx] = img->get_data();

    using bytes_ptr_t = decltype(port.texture.bytes);
    port.texture.bytes
        = reinterpret_cast<bytes_ptr_t>(input_buffers[tex_idx].ptrw());
    port.texture.width = img->get_width();
    port.texture.height = img->get_height();
    port.texture.changed = true;
  }

  void upload_textures()
  {
    int tex_idx = 0;
    avnd::cpu_texture_output_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      auto& port = avnd::pfr::get<Idx>(effect.outputs());
      auto& tex = port.texture;

      if(tex.changed && tex.bytes && tex.width > 0 && tex.height > 0)
      {
        upload_single_texture(tex, output_textures[tex_idx]);
        tex.changed = false;
      }
      ++tex_idx;
    });
  }

  template <typename Tex>
  void upload_single_texture(
      Tex& tex, godot::Ref<godot::ImageTexture>& img_tex)
  {
    const auto fmt = godot_image_format_of(tex);
    const int byte_count = godot_texture_bytesize(tex);

    godot::PackedByteArray pba;
    pba.resize(byte_count);
    memcpy(pba.ptrw(), reinterpret_cast<const unsigned char*>(tex.bytes),
        byte_count);

    auto img = godot::Image::create_from_data(
        tex.width, tex.height, false, fmt, pba);
    if(img.is_null())
      return;

    // ImageTexture::update requires identical size and format
    if(img_tex->get_width() != tex.width
        || img_tex->get_height() != tex.height
        || img_tex->get_format() != fmt)
    {
      img_tex->set_image(img);
    }
    else
    {
      img_tex->update(img);
    }
  }
};

}

// clang-format off
#define AVND_GODOT_TEXTURE_NODE_OVERRIDES                                                \
public:                                                                                  \
  void _ready() override { this->do_ready(); }                                           \
  void _process(double delta) override { this->do_process(delta); }                      \
  void _get_property_list(godot::List<godot::PropertyInfo>* p) const                     \
  { this->do_get_property_list(p); }                                                     \
  bool _set(const godot::StringName& n, const godot::Variant& v)                        \
  { return this->do_set(n, v); }                                                         \
  bool _get(const godot::StringName& n, godot::Variant& r) const                        \
  { return this->do_get(n, r); }                                                         \
  bool _property_can_revert(const godot::StringName& n) const                            \
  { return this->do_property_can_revert(n); }                                            \
  bool _property_get_revert(const godot::StringName& n, godot::Variant& r) const        \
  { return this->do_property_get_revert(n, r); }
// clang-format on
