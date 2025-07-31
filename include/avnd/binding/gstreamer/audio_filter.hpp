#pragma once

#include <avnd/binding/gstreamer/utils.hpp>

namespace gst
{

// Audio filter element for objects with audio input and output
template <typename T>
  requires(is_audio_filter<T>())
struct element<T>
{
  GstBaseTransform the_object; // MUST be first for GObject
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

  GstFlowReturn transform(GstBuffer* inbuf, GstBuffer* outbuf)
  {
    GST_DEBUG_OBJECT(this, "transform");

    GstMapInfo in_info, out_info;
    if(!gst_buffer_map(inbuf, &in_info, GST_MAP_READ))
    {
      GST_ERROR_OBJECT(this, "Failed to map input buffer");
      return GST_FLOW_ERROR;
    }

    if(!gst_buffer_map(outbuf, &out_info, GST_MAP_WRITE))
    {
      GST_ERROR_OBJECT(this, "Failed to map output buffer");
      gst_buffer_unmap(inbuf, &in_info);
      return GST_FLOW_ERROR;
    }

    // Ensure buffer sizes match
    if(in_info.size != out_info.size)
    {
      GST_ERROR_OBJECT(this, "Input and output buffer sizes don't match");
      gst_buffer_unmap(inbuf, &in_info);
      gst_buffer_unmap(outbuf, &out_info);
      return GST_FLOW_ERROR;
    }

    // For now, just do a simple pass-through with basic processing
    // TODO: Implement proper Avendish effect processing
    float* in_data = (float*)in_info.data;
    float* out_data = (float*)out_info.data;
    size_t num_samples = in_info.size / sizeof(float);

    // Apply a simple lowpass-like effect (basic implementation)
    static float prev_sample = 0.0f;
    const float alpha = 0.1f; // Simple lowpass coefficient

    for(size_t i = 0; i < num_samples; i++)
    {
      out_data[i] = alpha * in_data[i] + (1.0f - alpha) * prev_sample;
      prev_sample = out_data[i];
    }

    gst_buffer_unmap(inbuf, &in_info);
    gst_buffer_unmap(outbuf, &out_info);
    return GST_FLOW_OK;
  }

  // Stub caps negotiation methods (audio doesn't change format)
  GstCaps* transform_caps(GstPadDirection direction, GstCaps* caps, GstCaps* filter)
  {
    return gst_caps_ref(caps);
  }

  GstCaps* fixate_caps(GstPadDirection direction, GstCaps* caps, GstCaps* othercaps)
  {
    return gst_caps_fixate(gst_caps_copy(othercaps));
  }

  gboolean set_caps(GstCaps* incaps, GstCaps* outcaps) { return TRUE; }
};

template <typename T>
  requires(is_audio_filter<T>())
struct metaclass<T>
{
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate sink_pad_template = GST_STATIC_PAD_TEMPLATE(
      "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS("audio/x-raw,format=F32LE,channels=[1,8],rate=[8000,192000]"));

  static inline GstStaticPadTemplate src_pad_template = GST_STATIC_PAD_TEMPLATE(
      "src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS("audio/x-raw,format=F32LE,channels=[1,8],rate=[8000,192000]"));

  GstBaseTransformClass the_class;

  metaclass()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass* base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &sink_pad_template);
    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &src_pad_template);

    static constexpr auto c_name = avnd::get_static_symbol<T>();
    static std::string author = avnd::get_author<T>();
    if constexpr(!avnd::has_author<T>)
      author = "John Doe";
    static std::string_view desc = avnd::get_description<T>();
    if constexpr(!avnd::has_description<T>)
      desc = "Audio Filter";

    gst_element_class_set_static_metadata(
        GST_ELEMENT_CLASS(klass), std::string_view{c_name}.data(), "Filter/Effect/Audio",
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
      std::destroy_at((element<T>*)GST_BASE_TRANSFORM(gobject));
      G_OBJECT_CLASS(parent_class)->dispose(gobject);
    });

    // Transform API
    base_transform_class->transform = GST_DEBUG_FUNCPTR(
        +[](GstBaseTransform* gobject, GstBuffer* inbuf,
            GstBuffer* outbuf) -> GstFlowReturn {
      return ((element<T>*)gobject)->transform(inbuf, outbuf);
    });

    // Caps negotiation for texture processing
    base_transform_class->transform_caps = GST_DEBUG_FUNCPTR(
        +[](GstBaseTransform* gobject, GstPadDirection direction, GstCaps* caps,
            GstCaps* filter) -> GstCaps* {
      return ((element<T>*)gobject)->transform_caps(direction, caps, filter);
    });

    base_transform_class->fixate_caps = GST_DEBUG_FUNCPTR(
        +[](GstBaseTransform* gobject, GstPadDirection direction, GstCaps* caps,
            GstCaps* othercaps) -> GstCaps* {
      return ((element<T>*)gobject)->fixate_caps(direction, caps, othercaps);
    });

    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(
        +[](GstBaseTransform* gobject, GstCaps* incaps, GstCaps* outcaps) -> gboolean {
      return ((element<T>*)gobject)->set_caps(incaps, outcaps);
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
        gst_base_transform_get_type(),
        g_intern_static_string(std::string_view{c_name}.data()), sizeof(metaclass),
        class_intern_init_ptr, sizeof(element<T>), (GInstanceInitFunc)instance_init_ptr,
        (GTypeFlags)0);

    debug_category = _gst_debug_category_new(
        c_name.data(), 0, "debug category for audio filter element");

    return tid;
  };

  static GType get_type()
  {
    static const GType tid = get_type_once();
    return tid;
  }
};
}
