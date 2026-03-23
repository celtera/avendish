#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/godot/node.hpp>
#include <avnd/introspection/port.hpp>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace godot_binding
{

/// Map an Avendish texture format enum to a Godot Image::Format.
template <typename Tex>
constexpr godot::Image::Format godot_image_format()
{
  using format_t = typename Tex::format;
  if constexpr(std::is_same_v<format_t, typename halp::rgba_texture::format>)
    return godot::Image::FORMAT_RGBA8;
  else if constexpr(std::is_same_v<format_t, typename halp::rgb_texture::format>)
    return godot::Image::FORMAT_RGB8;
  else if constexpr(std::is_same_v<format_t, typename halp::r8_texture::format>)
    return godot::Image::FORMAT_R8;
  else if constexpr(std::is_same_v<format_t, typename halp::rgba32f_texture::format>)
    return godot::Image::FORMAT_RGBAF;
  else if constexpr(std::is_same_v<format_t, typename halp::r32f_texture::format>)
    return godot::Image::FORMAT_RF;
  else
    return godot::Image::FORMAT_RGBA8;
}

/**
 * Godot Node wrapping an Avendish texture generator/filter.
 *
 * Each frame, runs the processor's operator() and uploads output textures
 * to Godot ImageTexture resources. The textures are exposed as properties
 * so they can be assigned to Sprite2D, TextureRect, etc.
 */
template <typename T>
struct godot_texture_node : public godot::Node
{
  mutable avnd::effect_container<T> effect;

  static constexpr int tex_output_count
      = avnd::cpu_texture_output_introspection<T>::size;

  godot::Ref<godot::ImageTexture> output_textures[tex_output_count > 0
                                                       ? tex_output_count
                                                       : 1];

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

    // Expose output textures as read-only properties
    int tex_idx = 0;
    avnd::cpu_texture_output_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      using outputs_t = typename avnd::outputs_type<T>::type;
      auto tex_name
          = godot::StringName(field_name<Idx, C, outputs_t>().data());
      p_list->push_back(godot::PropertyInfo(
          godot::Variant::OBJECT, tex_name, godot::PROPERTY_HINT_RESOURCE_TYPE,
          "ImageTexture"));
      ++tex_idx;
    });
  }

  bool do_set(const godot::StringName& p_name, const godot::Variant& p_value)
  {
    return godot_binding::set_property<T>(effect, p_name, p_value);
  }

  bool do_get(const godot::StringName& p_name, godot::Variant& r_ret) const
  {
    // Check output textures first
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
    const int byte_count = tex.width * tex.height * Tex::bytes_per_pixel;

    godot::PackedByteArray pba;
    pba.resize(byte_count);
    memcpy(pba.ptrw(), reinterpret_cast<const unsigned char*>(tex.bytes),
        byte_count);

    auto img = godot::Image::create_from_data(
        tex.width, tex.height, false, godot_image_format<Tex>(), pba);

    if(img_tex->get_width() != tex.width
        || img_tex->get_height() != tex.height)
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
