#pragma once
#include <avnd/binding/gstreamer/parameters.hpp>
#include <avnd/common/init_cpp_with_c_object_header.hpp>
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/process_execution.hpp>
#include <gst/audio/audio.h>
#include <gst/base/gstbasesink.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstpushsrc.h>
#include <gst/gst.h>
#include <gst/video/video.h>

namespace gst
{
static inline gpointer parent_class = nullptr;

template <typename T>
struct element
{
};
template <typename T>
struct metaclass;

struct get_property
{
  GValue* value{};
  GParamSpec* pspec{};
  void operator()(auto& param, const avnd::string_ish auto& v)
  {
    g_value_set_string(value, std::string_view(v).data());
  }
  void operator()(auto& param, const avnd::enum_ish auto& v)
  {
    // FIXME
  }
  void operator()(auto& param, const avnd::optional_ish auto& v)
  {
    // FIXME
  }
  void operator()(auto& param, const avnd::variant_ish auto& v)
  {
    // FIXME
  }
  void operator()(auto& param, const avnd::iterable_ish auto& v)
  {
    // FIXME
  }
  void operator()(auto& param, const int& v) { g_value_set_int(value, v); }
  void operator()(auto& param, const unsigned int& v) { g_value_set_uint(value, v); }
  void operator()(auto& param, const int64_t& v) { g_value_set_int64(value, v); }
  void operator()(auto& param, const uint64_t& v) { g_value_set_uint64(value, v); }
  void operator()(auto& param, const float& v) { g_value_set_float(value, v); }
  void operator()(auto& param, const double& v) { g_value_set_double(value, v); }
  void operator()(auto& param, const bool& v) { g_value_set_boolean(value, v); }
  void operator()(auto& param, const char*& v) { g_value_set_string(value, v); }

  void operator()(auto& param) { (*this)(param, param.value); }
};
struct set_property
{
  const GValue* value{};
  GParamSpec* pspec{};
  void operator()(auto& param) { (*this)(param, param.value); }
  void operator()(auto& param, avnd::string_ish auto& v)
  {
    v = g_value_get_string(value);
  }
  void operator()(auto& param, avnd::enum_ish auto& v)
  {
    // FIXME
  }
  void operator()(auto& param, avnd::optional_ish auto& v)
  {
    // FIXME
  }
  void operator()(auto& param, avnd::variant_ish auto& v)
  {
    // FIXME
  }
  void operator()(auto& param, avnd::iterable_ish auto& v)
  {
    // FIXME
  }
  void operator()(auto& param, int& v) { v = g_value_get_int(value); }
  void operator()(auto& param, unsigned int& v) { v = g_value_get_uint(value); }
  void operator()(auto& param, int64_t& v) { v = g_value_get_int64(value); }
  void operator()(auto& param, uint64_t& v) { v = g_value_get_uint64(value); }
  void operator()(auto& param, float& v) { v = g_value_get_float(value); }
  void operator()(auto& param, double& v) { v = g_value_get_double(value); }
  void operator()(auto& param, bool& v) { v = g_value_get_boolean(value); }
  void operator()(auto& param, const char*& v) { v = g_value_get_string(value); }
};

// Helper functions to determine element types
template <typename T>
constexpr bool is_audio_filter()
{
  return avnd::audio_bus_input_introspection<T>::size > 0
         && avnd::audio_bus_output_introspection<T>::size > 0;
}

template <typename T>
constexpr bool is_texture_generator()
{
  return avnd::texture_input_introspection<T>::size == 0
         && avnd::texture_output_introspection<T>::size > 0;
}

template <typename T>
constexpr bool is_texture_filter()
{
  return avnd::texture_input_introspection<T>::size > 0
         && avnd::texture_output_introspection<T>::size > 0;
}

template <typename T>
constexpr bool is_texture_sink()
{
  return avnd::texture_input_introspection<T>::size > 0
         && avnd::texture_output_introspection<T>::size == 0;
}

template <typename T>
constexpr bool is_audio_generator()
{
  return avnd::audio_bus_input_introspection<T>::size == 0
         && avnd::audio_bus_output_introspection<T>::size > 0;
}

template <typename T>
constexpr bool is_audio_sink()
{
  return avnd::audio_bus_input_introspection<T>::size > 0
         && avnd::audio_bus_output_introspection<T>::size == 0;
}

// Common property handling using C++23 deducing this
struct property_handler
{
  // Using deducing this to work with any derived element type
  template <typename Self>
  void set_property(
      this Self& self, guint property_id, const GValue* value, GParamSpec* pspec)
  {
    GST_DEBUG_OBJECT(&self, "set_property");

    if(property_id == 0)
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(&self, property_id, pspec);
      return;
    }

    using T = typename Self::effect_type;
    avnd::parameter_input_introspection<T>::for_nth_mapped(
        avnd::get_inputs(self.impl), property_id - 1, gst::set_property{value, pspec});
  }

  template <typename Self>
  void get_property(this Self& self, guint property_id, GValue* value, GParamSpec* pspec)
  {
    GST_DEBUG_OBJECT(&self, "get_property");

    if(property_id == 0)
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(&self, property_id, pspec);
      return;
    }

    using T = typename Self::effect_type;
    avnd::parameter_input_introspection<T>::for_nth_mapped(
        avnd::get_inputs(self.impl), property_id - 1, gst::get_property{value, pspec});
  }
};

// Common caps handling using deducing this
struct caps_handler
{
  template <typename Self>
  gboolean start(this Self&& self)
  {
    GST_DEBUG_OBJECT(&self, "start");
    return TRUE;
  }
};

template <typename Self>
static void init_properties(Self& self, std::string_view classification)
{
  using T = typename Self::type;
  static constexpr auto c_name = avnd::get_static_symbol<T>();
  static std::string author = avnd::get_author<T>();
  static std::string_view desc = avnd::get_description<T>();

  GObjectClass* gobject_class = G_OBJECT_CLASS(&self);
  gst_element_class_set_static_metadata(
      GST_ELEMENT_CLASS(&self), std::string_view{c_name}.data(), classification.data(),
      desc.data(), author.c_str());

  // GObject API
  gobject_class->set_property = +[](GObject* object, guint property_id,
                                    const GValue* value, GParamSpec* pspec) -> void {
    ((element<T>*)object)->set_property(property_id, value, pspec);
  };
  gobject_class->get_property = +[](GObject* object, guint property_id, GValue* value,
                                    GParamSpec* pspec) -> void {
    ((element<T>*)object)->get_property(property_id, value, pspec);
  };
  gobject_class->dispose = GST_DEBUG_FUNCPTR(+[](GObject* gobject) -> void {
    std::destroy_at((element<T>*)gobject);
    G_OBJECT_CLASS(parent_class)->dispose(gobject);
  });

  if constexpr(avnd::parameter_input_introspection<T>::size > 0)
  {
    avnd::parameter_input_introspection<T>::for_all(
        [gobject_class]<std::size_t I, typename F>(avnd::field_reflection<I, F> f) {
      static constexpr auto idx
          = avnd::parameter_input_introspection<T>::template unmap<I>();

      g_object_class_install_property(gobject_class, idx + 1, param_spec<F>());
    });
  }
}

template <typename T>
static GType get_type_once(GType base)
{
  static constexpr GClassInitFunc class_intern_init_ptr
      = +[](gpointer klass, gpointer class_data) {
    parent_class = g_type_class_peek_parent(klass);
    avnd::init_cpp_object<metaclass<T>, &metaclass<T>::the_class>(klass);
  };

  static constexpr GInstanceInitFunc instance_init_ptr
      = +[](GTypeInstance* ptr, void* cls) {
    avnd::init_cpp_object<element<T>, &element<T>::the_object>(ptr);
  };

  static constexpr auto c_name = avnd::get_static_symbol<T>();

  const GType tid = g_type_register_static_simple(
      base, g_intern_static_string(std::string_view{c_name}.data()),
      sizeof(metaclass<T>), class_intern_init_ptr, sizeof(element<T>),
      (GInstanceInitFunc)instance_init_ptr, (GTypeFlags)0);

  metaclass<T>::debug_category
      = _gst_debug_category_new(c_name.data(), 0, "debug category for avendish element");

  return tid;
}

// Base class that combines common functionality
template <typename T>
struct element_common
    : property_handler
    , caps_handler
{
  using effect_type = T;
  avnd::effect_container<T> impl;
  avnd::process_adapter<T> processor;
};

}
