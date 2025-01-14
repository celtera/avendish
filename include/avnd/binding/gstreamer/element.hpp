#pragma once

#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <gst/base/gstpushsrc.h>
#include <gst/gst.h>

namespace gst
{
static inline gpointer parent_class = nullptr;
template <typename T>
struct element;

template <typename T>
static element<T>* self(void* ptr);

struct get_property
{
  GValue* value{};
  GParamSpec* pspec{};
  void operator()(auto& param, const avnd::string_ish auto& v)
  {
    g_value_set_string(value, std::string_view(v).data());
  }
  void operator()(auto& param, const avnd::enum_ish auto& v) { }
  void operator()(auto& param, const int& v) { g_value_set_int(value, v); }
  void operator()(auto& param, const unsigned int& v) { g_value_set_uint(value, v); }
  void operator()(auto& param, const int64_t& v) { g_value_set_int64(value, v); }
  void operator()(auto& param, const uint64_t& v) { g_value_set_uint64(value, v); }
  void operator()(auto& param, const float& v) { g_value_set_float(value, v); }
  void operator()(auto& param, const double& v) { g_value_set_double(value, v); }
  void operator()(auto& param, const bool& v) { g_value_set_boolean(value, v); }

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
  void operator()(auto& param, avnd::enum_ish auto& v) { }
  void operator()(auto& param, int& v) { v = g_value_get_int(value); }
  void operator()(auto& param, unsigned int& v) { v = g_value_get_uint(value); }
  void operator()(auto& param, int64_t& v) { v = g_value_get_int64(value); }
  void operator()(auto& param, uint64_t& v) { v = g_value_get_uint64(value); }
  void operator()(auto& param, float& v) { v = g_value_get_float(value); }
  void operator()(auto& param, double& v) { v = g_value_get_double(value); }
  void operator()(auto& param, bool& v) { v = g_value_get_boolean(value); }
};

template <typename T>
struct element
{
  GstPushSrc the_object;
  avnd::effect_container<T> impl;

  element() { }

  ~element() { }

  void set_property(guint property_id, const GValue* value, GParamSpec* pspec)
  {
    GST_DEBUG_OBJECT(this, "set_property");

    if(property_id == 0)
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(this, property_id, pspec);
      return;
    }

    avnd::parameter_input_introspection<T>::for_nth_mapped(
        avnd::get_inputs(impl), property_id - 1, gst::set_property{value, pspec});
  }

  void get_property(guint property_id, GValue* value, GParamSpec* pspec)
  {
    GST_DEBUG_OBJECT(this, "get_property");

    if(property_id == 0)
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(this, property_id, pspec);
      return;
    }

    avnd::parameter_input_introspection<T>::for_nth_mapped(
        avnd::get_inputs(impl), property_id - 1, gst::get_property{value, pspec});
  }

  gboolean start()
  {
    GST_DEBUG_OBJECT(this, "start");

    return TRUE;
  }

  GstFlowReturn fill(GstBuffer* buf)
  {
    GST_DEBUG_OBJECT(this, "fill");
    GstMapInfo info;
    gst_buffer_map(buf, &info, GST_MAP_WRITE);
    gsize buf_size = gst_buffer_get_size(buf);
    for(gsize i = 0; i < buf_size; i++)
      info.data[i] = rand();

    gst_buffer_unmap(buf, &info);
    return GST_FLOW_OK;
  }
};

template <typename F>
  requires(avnd::has_range<F> && avnd::enum_ish_parameter<F>)
static GParamSpec* param_spec()
{
  static constexpr auto symname = avnd::get_static_symbol<F>();
  static constexpr std::string_view name = avnd::get_name<F>();
  static constexpr std::string_view desc = avnd::get_description<F>();

  using V = decltype(F::value);
  static constexpr auto range = avnd::get_range<F>();
  int flags = G_PARAM_READWRITE;
  if constexpr(std::is_same_v<float, V>)
  {
    return g_param_spec_float(
        symname.data(), name.data(), desc.data(), 0.f, 1.f, range.init,
        static_cast<GParamFlags>(flags));
  }
  else
  {
    return nullptr;
  }
}

template <typename F>
  requires(!avnd::has_range<F>)
static GParamSpec* param_spec()
{
  static constexpr auto symname = avnd::get_static_symbol<F>();
  static constexpr std::string_view name = avnd::get_name<F>();
  static constexpr std::string_view desc = avnd::get_description<F>();

  using V = decltype(F::value);
  int flags = G_PARAM_READWRITE;
  if constexpr(std::is_same_v<float, V>)
  {
    return g_param_spec_float(
        symname.data(), name.data(), desc.data(), 0.f, 1.f, 0.f,
        static_cast<GParamFlags>(flags));
  }
  else
  {
    return nullptr;
  }
}

template <typename F>
  requires(avnd::has_range<F> && !avnd::enum_ish_parameter<F>)
static GParamSpec* param_spec()
{
  static constexpr auto symname = avnd::get_static_symbol<F>();
  static constexpr std::string_view name = avnd::get_name<F>();
  static constexpr std::string_view desc = avnd::get_description<F>();

  using V = decltype(F::value);
  static constexpr auto range = avnd::get_range<F>();
  int flags = G_PARAM_READWRITE;
  if constexpr(std::is_same_v<int, V>)
  {
    return g_param_spec_int(
        symname.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<unsigned int, V>)
  {
    return g_param_spec_uint(
        symname.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<long, V>)
  {
    return g_param_spec_long(
        symname.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<unsigned long, V>)
  {
    return g_param_spec_ulong(
        symname.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<int64_t, V>)
  {
    return g_param_spec_int64(
        symname.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<uint64_t, V>)
  {
    return g_param_spec_uint64(
        symname.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<float, V>)
  {
    return g_param_spec_float(
        symname.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(std::is_same_v<double, V>)
  {
    return g_param_spec_double(
        symname.data(), name.data(), desc.data(), range.min, range.max, range.init,
        static_cast<GParamFlags>(flags));
  }
  else if constexpr(avnd::string_ish<V>)
  {
    return g_param_spec_string(
        symname.data(), name.data(), desc.data(), std::string_view(range.init).data(),
        static_cast<GParamFlags>(flags | G_PARAM_STATIC_STRINGS));
  }
  else
  {
    return nullptr;
  }
}

template <typename T>
struct metaclass
{
  static inline GstDebugCategory* debug_category = nullptr;

  // FIXME
  static inline GstStaticPadTemplate pad_template = GST_STATIC_PAD_TEMPLATE(
      "src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS("video/x-raw,format=RGB,width=640,height=480"));

  GstPushSrcClass the_class;

  metaclass()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstPushSrcClass* push_src_class = GST_PUSH_SRC_CLASS(klass);
    GstBaseSrcClass* base_src_class = GST_BASE_SRC_CLASS(klass);

    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass), &pad_template);

    static constexpr auto c_name = avnd::get_static_symbol<T>();
    static std::string author = avnd::get_author<T>();
    if constexpr(!avnd::has_author<T>)
      author = "John Doe";
    static std::string_view desc = avnd::get_description<T>();
    if constexpr(!avnd::has_description<T>)
      desc = "Description";

    g_print("instantiate");
    gst_element_class_set_static_metadata(
        GST_ELEMENT_CLASS(klass), std::string_view{c_name}.data(), "Generic",
        desc.data(), author.c_str());

    // GObject API
    gobject_class->set_property = +[](GObject* object, guint property_id,
                                      const GValue* value, GParamSpec* pspec) -> void {
      self<T>(object)->set_property(property_id, value, pspec);
    };
    gobject_class->get_property = +[](GObject* object, guint property_id, GValue* value,
                                      GParamSpec* pspec) -> void {
      self<T>(object)->get_property(property_id, value, pspec);
    };
    gobject_class->dispose = GST_DEBUG_FUNCPTR(+[](GObject* gobject) -> void {
      std::destroy_at(self<T>(GST_PUSH_SRC(gobject)));

      G_OBJECT_CLASS(parent_class)->dispose(gobject);
    });

    // GStreamer API
    base_src_class->start = GST_DEBUG_FUNCPTR(
        +[](GstBaseSrc* gobject) -> gboolean { return self<T>(gobject)->start(); });

    push_src_class->fill
        = GST_DEBUG_FUNCPTR(+[](GstPushSrc* gobject, GstBuffer* buf) -> GstFlowReturn {
      return self<T>(gobject)->fill(buf);
    });

    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      avnd::parameter_input_introspection<T>::for_all(
          [gobject_class]<std::size_t I, typename F>(avnd::field_reflection<I, F> f) {
        static constexpr auto idx
            = avnd::parameter_input_introspection<T>::template unmap<I>();

        auto param = param_spec<F>();
        if(!param)
        {
          static constexpr auto symname = avnd::get_static_symbol<F>();
          static constexpr std::string_view name = avnd::get_name<F>();
          static constexpr std::string_view desc = avnd::get_description<F>();

          param = g_param_spec_string(
              symname.data(), name.data(), desc.data(), "",
              static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
        }
        g_object_class_install_property(gobject_class, idx + 1, param);
      });
    }
  }

  static GType get_type_once()
  {
    static constexpr GClassInitFunc class_intern_init_ptr
        = +[](gpointer klass, gpointer class_data) {
      parent_class = g_type_class_peek_parent(klass);
      std::construct_at((metaclass*)klass);
    };

    static constexpr GInstanceInitFunc instance_init_ptr
        = +[](GTypeInstance* ptr, void* cls) { std::construct_at((element<T>*)ptr); };

    static constexpr auto c_name = avnd::get_static_symbol<T>();

    const GType tid = g_type_register_static_simple(
        gst_push_src_get_type(), g_intern_static_string(std::string_view{c_name}.data()),
        sizeof(metaclass), class_intern_init_ptr, sizeof(element<T>),
        (GInstanceInitFunc)instance_init_ptr, (GTypeFlags)0);

    debug_category
        = _gst_debug_category_new(c_name.data(), 0, "debug category for element");

    return tid;
  };

  static GType get_type()
  {
    static const GType tid = get_type_once();
    return tid;
  }
};

template <typename T>
static element<T>* self(void* ptr)
{
  return (element<T>*)g_type_check_instance_cast(
      (GTypeInstance*)(ptr), metaclass<T>::get_type());
}
}
